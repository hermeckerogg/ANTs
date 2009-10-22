/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile: ClusterImageStatistics.cxx,v $
  Language:  C++
  Date:      $Date: 2008/12/15 16:35:13 $
  Version:   $Revision: 1.20 $

  Copyright (c) 2002 Insight Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "itkDiscreteGaussianImageFilter.h"

//  RecursiveAverageImages img1  img2 weightonimg2 outputname

// We divide the 2nd input image by its mean and add it to the first
// input image with weight 1/n.
// The output overwrites the 1st img with the sum.

#include <list>
#include <vector>
#include <fstream>
#include "vnl/vnl_vector.h"

#include "itkMinimumMaximumImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkRelabelComponentImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkLabelStatisticsImageFilter.h"

#include  "ReadWriteImage.h"

template <unsigned int ImageDimension>
int  ClusterStatistics(unsigned int argc, char *argv[])
{
  typedef float PixelType;
//  const unsigned int ImageDimension = AvantsImageDimension;
  typedef itk::Vector<float, ImageDimension>                              VectorType;
  typedef itk::Image<VectorType, ImageDimension>                          FieldType;
  typedef itk::Image<PixelType, ImageDimension>                           ImageType;
  typedef itk::ImageFileReader<ImageType>                                 readertype;
  typedef itk::ImageFileWriter<ImageType>                                 writertype;
  typedef typename ImageType::IndexType                                   IndexType;
  typedef typename ImageType::SizeType                                    SizeType;
  typedef typename ImageType::SpacingType                                 SpacingType;
  typedef itk::AffineTransform<double, ImageDimension>                    AffineTransformType;
  typedef itk::LinearInterpolateImageFunction<ImageType, double>          InterpolatorType1;
  typedef itk::NearestNeighborInterpolateImageFunction<ImageType, double> InterpolatorType2;
  // typedef itk::ImageRegionIteratorWithIndex<ImageType> Iterator;

  typedef float                                                            InternalPixelType;
  typedef unsigned long                                                    ULPixelType;
  typedef itk::Image<ULPixelType, ImageDimension>                          labelimagetype;
  typedef ImageType                                                        InternalImageType;
  typedef ImageType                                                        OutputImageType;
  typedef itk::ConnectedComponentImageFilter<ImageType, labelimagetype>    FilterType;
  typedef itk::RelabelComponentImageFilter<labelimagetype, labelimagetype> RelabelType;

  // want the average value in each cluster as defined by the mask and the value thresh and the clust thresh

  std::string roimaskfn = std::string(argv[2]);
  std::string labelimagefn = std::string(argv[3]);
  std::string outname = std::string(argv[4]);
  float       clusterthresh = atof(argv[5]);
  float       minSize = clusterthresh;
  float       valuethresh = atof(argv[6]);
  //  std::cout << " Cth " << clusterthresh << " Vth " << valuethresh << std::endl;
  typename ImageType::Pointer valimage = NULL;
  typename ImageType::Pointer roiimage = NULL;
  typename ImageType::Pointer labelimage = NULL;

  ReadImage<ImageType>(roiimage, roimaskfn.c_str() );
  ReadImage<ImageType>(labelimage, labelimagefn.c_str() );

  typedef itk::MinimumMaximumImageFilter<ImageType> MinMaxFilterType;
  typename MinMaxFilterType::Pointer minMaxFilter = MinMaxFilterType::New();
  minMaxFilter->SetInput( labelimage );
  minMaxFilter->Update();
  double min = minMaxFilter->GetMinimum();
  double max = minMaxFilter->GetMaximum();
  double range = max - min;
  for( unsigned int filecount = 7;  filecount < argc; filecount++ )
    {
    //    std::cout <<" doing " << std::string(argv[filecount]) << std::endl;

    ReadImage<ImageType>(valimage, argv[filecount]);

    //  first, threshold the value image then get the clusters of min size
    typedef itk::BinaryThresholdImageFilter<ImageType, ImageType> ThresholdFilterType;
    typename ThresholdFilterType::Pointer threshold = ThresholdFilterType::New();
    threshold->SetInput(valimage);
    threshold->SetInsideValue(1);
    threshold->SetOutsideValue(0);
    threshold->SetLowerThreshold(valuethresh);
    threshold->SetUpperThreshold(1.e9);
    threshold->Update();
    typename ImageType::Pointer thresh = threshold->GetOutput();
    typedef itk::ImageRegionIteratorWithIndex<ImageType>      fIterator;
    typedef itk::ImageRegionIteratorWithIndex<labelimagetype> Iterator;
    fIterator tIter( thresh, thresh->GetLargestPossibleRegion() );
    for(  tIter.GoToBegin(); !tIter.IsAtEnd(); ++tIter )
      {
      if( roiimage->GetPixel(tIter.GetIndex() ) < 0.5 )
        {
        tIter.Set(0);
        }
      }

    //  typename
    typename FilterType::Pointer filter = FilterType::New();
// typename
    typename RelabelType::Pointer relabel = RelabelType::New();

    filter->SetInput( thresh );
    int fullyConnected = 0; // atoi( argv[5] );
    filter->SetFullyConnected( fullyConnected );
    relabel->SetInput( filter->GetOutput() );
    relabel->SetMinimumObjectSize( (unsigned int) minSize );

    try
      {
      relabel->Update();
      }
    catch( itk::ExceptionObject & excep )
      {
      std::cerr << "Relabel: exception caught !" << std::endl;
      std::cerr << excep << std::endl;
      }

    typename ImageType::Pointer Clusters = MakeNewImage<ImageType>(valimage, 0);
    typename ImageType::Pointer Values = MakeNewImage<ImageType>(valimage, 0);
    typename ImageType::Pointer Labels = MakeNewImage<ImageType>(valimage, 0);
    Iterator vfIter( relabel->GetOutput(),  relabel->GetOutput()->GetLargestPossibleRegion() );

    float maximum = relabel->GetNumberOfObjects();
    //    std::cout << " #object " << maximum << std::endl;
//    float maxtstat=0;
    std::vector<unsigned long> histogram( (int)maximum + 1);
    std::vector<long>          maxlabel( (int)maximum + 1);
    std::vector<float>         suminlabel( (unsigned long) range + 1);
    std::vector<unsigned long> countinlabel( (unsigned long) range + 1);
    std::vector<float>         sumofvalues( (int)maximum + 1);
    std::vector<float>         maxvalue( (int)maximum + 1);
    for( int i = 0; i <= maximum; i++ )
      {
      histogram[i] = 0;
      sumofvalues[i] = 0;
      maxvalue[i] = 0;
      maxlabel[i] = 0;
      }
    for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
      {
      if( vfIter.Get() > 0 )
        {
        float vox = valimage->GetPixel(vfIter.GetIndex() );
        if( vox >= valuethresh )
          {
          histogram[(unsigned long)vfIter.Get()] = histogram[(unsigned long)vfIter.Get()] + 1;
          sumofvalues[(unsigned long)vfIter.Get()] = sumofvalues[(unsigned long)vfIter.Get()] + vox;
          if( maxvalue[(unsigned long)vfIter.Get()]  < vox )
            {
            maxvalue[(unsigned long)vfIter.Get()] = vox;
            maxlabel[(unsigned long)vfIter.Get()] = (long int)labelimage->GetPixel(vfIter.GetIndex() );
            }

          suminlabel[(unsigned long)(labelimage->GetPixel(vfIter.GetIndex() ) - min)] += vox;
          countinlabel[(unsigned long)(labelimage->GetPixel(vfIter.GetIndex() ) - min)] += 1;
          }
        }
      }
    for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
      {
      if( vfIter.Get() > 0 )
        {
        Clusters->SetPixel( vfIter.GetIndex(), histogram[(unsigned long)vfIter.Get()]  );
        Values->SetPixel( vfIter.GetIndex(), sumofvalues[(unsigned long)vfIter.Get()]
                          / (float)histogram[(unsigned int)vfIter.Get()]  );
        Labels->SetPixel( vfIter.GetIndex(), labelimage->GetPixel(vfIter.GetIndex() ) );
        }
      else
        {
        Clusters->SetPixel(vfIter.GetIndex(), 0);
        Labels->SetPixel(vfIter.GetIndex(), 0);
        Values->SetPixel(vfIter.GetIndex(), 0);
        }
      }

    //  WriteImage<ImageType>(Values,std::string("temp.nii.gz").c_str());
    // WriteImage<ImageType>(Clusters,std::string("temp2.nii.gz").c_str());

    float maximgval = 0;
    for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
      {
      if( Clusters->GetPixel( vfIter.GetIndex() ) > maximgval )
        {
        maximgval = Clusters->GetPixel( vfIter.GetIndex() );
        }
      }
    //  std::cout << " max size " << maximgval << std::endl;
    for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
      {
      if( Clusters->GetPixel( vfIter.GetIndex() ) < minSize )
        {
        Clusters->SetPixel( vfIter.GetIndex(), 0);
        Values->SetPixel( vfIter.GetIndex(), 0);
        Labels->SetPixel( vfIter.GetIndex(), 0);
        }
      }

//  WriteImage<ImageType>(Values,(outname+"values.nii.gz").c_str());
//  WriteImage<ImageType>(Labels,(outname+"labels.nii.gz").c_str());
    WriteImage<ImageType>(Clusters, (outname + "sizes.nii.gz").c_str() );

    // now begin output
    //  std::cout << " Writing Text File " << outname << std::endl;
    std::ofstream outf( (outname).c_str(), std::ofstream::app);
    if( outf.good() )
      {
      //    outf << std::string(argv[filecount]) << std::endl;
      for( int i = 0; i < maximum + 1; i++ )
        {
        if( histogram[i] >= minSize )
          {
          //	      outf << " Cluster " << i << " size  " << histogram[i] <<  " average " <<
          // sumofvalues[i]/(float)histogram[i] << " max " << maxvalue[i] << " label " <<  maxlabel[i] <<  std::endl;
          if( i >= 0 && i < maximum )
            {
            outf << sumofvalues[i] / (float)histogram[i] << ",";
            }
          else
            {
            outf << sumofvalues[i] / (float)histogram[i] << std::endl;
            }
          std::cout << " Cluster " << i << " size  " << histogram[i] <<  " average " << sumofvalues[i]
          / (float)histogram[i] << " max " << maxvalue[i] << " label " <<  maxlabel[i] <<  std::endl;
          }
        }
      for( unsigned int i = 0; i <= range; i++ )
        {
        if( countinlabel[i] > 0 )
          {
          //	      outf << " Label " << i+min <<   " average " << suminlabel[i]/(float)countinlabel[i] <<  std::endl;
          std::cout << " Label " << i + min <<   " average " << suminlabel[i] / (float)countinlabel[i] <<  std::endl;
          }
        }
      }
    else
      {
      std::cout << " File No Good! " << outname << std::endl;
      }
    outf.close();
    }

  return 0;
}

int main(int argc, char *argv[])
{
  if( argc < 4 )
    {
    std::cout
    <<
    " Given an ROI and Label Image, find the max and average value   \n in a value image  where the value > some user-defined threshold \n and the cluster size  is larger than some min size. \n "
    << std::endl;
    std::cout << "Useage ex: \n  " << std::endl;
    std::cout << argv[0]
              <<
    "  ImageDimension ROIMask.ext LabelImage.ext  OutPrefix   MinimumClusterSize  ValueImageThreshold  Image1WithValuesOfInterest.ext ...  ImageNWithValuesOfInterest.ext  \n \n "
              << std::endl;
    std::cout
    <<
    " ROIMask.ext -- overall region of interest \n  \n LabelImage.ext -- labels for the sub-regions, e.g. Brodmann or just unique labels (see  LabelClustersUniquely ) \n \n  OutputPrefix -- all output  has this prefix  \n \n  MinimumClusterSize -- the minimum size of clusters of interest  \n  \n ValueImageThreshold -- minimum value of interest \n \n   Image*WithValuesOfInterest.ext  ---  image(s) that define the values you want to measure \n ";
    return 1;
    }

  switch( atoi(argv[1]) )
    {
    case 2:
      {
      ClusterStatistics<2>(argc, argv);
      }
      break;
    case 3:
      {
      ClusterStatistics<3>(argc, argv);
      }
      break;
    default:
      std::cerr << "Unsupported dimension" << std::endl;
      exit( EXIT_FAILURE );
    }

  return 0;
}
