// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_utils.h"

#include <map>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ParallelDownloadUtilsTest, FindSlicesToDownload) {
  std::vector<DownloadItem::ReceivedSlice> downloaded_slices;
  std::vector<DownloadItem::ReceivedSlice> slices_to_download =
      FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(0, slices_to_download[0].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  downloaded_slices.emplace_back(0, 500);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(500, slices_to_download[0].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  // Create a gap between slices.
  downloaded_slices.emplace_back(1000, 500);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(2u, slices_to_download.size());
  EXPECT_EQ(500, slices_to_download[0].offset);
  EXPECT_EQ(500, slices_to_download[0].received_bytes);
  EXPECT_EQ(1500, slices_to_download[1].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[1].received_bytes);

  // Fill the gap.
  downloaded_slices.emplace(
      downloaded_slices.begin() + 1, slices_to_download[0]);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(1500, slices_to_download[0].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  // Create a new gap at the beginning.
  downloaded_slices.erase(downloaded_slices.begin());
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(2u, slices_to_download.size());
  EXPECT_EQ(0, slices_to_download[0].offset);
  EXPECT_EQ(500, slices_to_download[0].received_bytes);
  EXPECT_EQ(1500, slices_to_download[1].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[1].received_bytes);
}

TEST(ParallelDownloadUtilsTest, AddOrMergeReceivedSliceIntoSortedArray) {
  std::vector<DownloadItem::ReceivedSlice> slices;
  DownloadItem::ReceivedSlice slice1(500, 500);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice1, slices));
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(slice1, slices[0]);

  // Adding a slice that can be merged with existing slice.
  DownloadItem::ReceivedSlice slice2(1000, 400);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice2, slices));
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(500, slices[0].offset);
  EXPECT_EQ(900, slices[0].received_bytes);

  DownloadItem::ReceivedSlice slice3(0, 50);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice3, slices));
  EXPECT_EQ(2u, slices.size());
  EXPECT_EQ(slice3, slices[0]);

  DownloadItem::ReceivedSlice slice4(100, 50);
  EXPECT_EQ(1u, AddOrMergeReceivedSliceIntoSortedArray(slice4, slices));
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(slice3, slices[0]);
  EXPECT_EQ(slice4, slices[1]);

  // A new slice can only merge with an existing slice earlier in the file, not
  // later in the file.
  DownloadItem::ReceivedSlice slice5(50, 50);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice5, slices));
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(100, slices[0].received_bytes);
  EXPECT_EQ(slice4, slices[1]);
}

// Ensure the minimum slice size is correctly applied.
TEST(ParallelDownloadUtilsTest, FindSlicesForRemainingContentMinSliceSize) {
  // Minimum slice size is smaller than total length, only one slice returned.
  DownloadItem::ReceivedSlices slices =
      FindSlicesForRemainingContent(0, 100, 3, 150);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(0, slices[0].received_bytes);

  // Request count is large, the minimum slice size should limit the number of
  // slices returned.
  slices = FindSlicesForRemainingContent(0, 100, 33, 50);
  EXPECT_EQ(2u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(50, slices[0].received_bytes);
  EXPECT_EQ(50, slices[1].offset);
  EXPECT_EQ(0, slices[1].received_bytes);

  // Can chunk 2 slices under minimum slice size, but request count is only 1,
  // request count should win.
  slices = FindSlicesForRemainingContent(0, 100, 1, 50);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(0, slices[0].received_bytes);

  // A total 100 bytes data and a 51 bytes minimum slice size, only one slice is
  // returned.
  slices = FindSlicesForRemainingContent(0, 100, 3, 51);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(0, slices[0].received_bytes);
}

TEST(ParallelDownloadUtilsTest, GetMaxContiguousDataBlockSizeFromBeginning) {
  std::vector<DownloadItem::ReceivedSlice> slices;
  slices.emplace_back(500, 500);
  EXPECT_EQ(0, GetMaxContiguousDataBlockSizeFromBeginning(slices));

  DownloadItem::ReceivedSlice slice1(0, 200);
  AddOrMergeReceivedSliceIntoSortedArray(slice1, slices);
  EXPECT_EQ(200, GetMaxContiguousDataBlockSizeFromBeginning(slices));

  DownloadItem::ReceivedSlice slice2(200, 300);
  AddOrMergeReceivedSliceIntoSortedArray(slice2, slices);
  EXPECT_EQ(1000, GetMaxContiguousDataBlockSizeFromBeginning(slices));
}

// Test to verify Finch parameters for enabled experiment group is read
// correctly.
TEST(ParallelDownloadUtilsTest, FinchConfigEnabled) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> params = {
      {content::kMinSliceSizeFinchKey, "1234"},
      {content::kParallelRequestCountFinchKey, "6"},
      {content::kParallelRequestDelayFinchKey, "2000"},
      {content::kParallelRequestRemainingTimeFinchKey, "3"}};
  feature_list.InitAndEnableFeatureWithParameters(
      features::kParallelDownloading, params);
  EXPECT_TRUE(IsParallelDownloadEnabled());
  EXPECT_EQ(GetMinSliceSizeConfig(), 1234);
  EXPECT_EQ(GetParallelRequestCountConfig(), 6);
  EXPECT_EQ(GetParallelRequestDelayConfig(), base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(GetParallelRequestRemainingTimeConfig(),
            base::TimeDelta::FromSeconds(3));
}

// Test to verify the disable experiment group will actually disable the
// feature.
TEST(ParallelDownloadUtilsTest, FinchConfigDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kParallelDownloading);
  EXPECT_FALSE(IsParallelDownloadEnabled());
}

// Test to verify that the Finch parameter |enable_parallel_download| works
// correctly.
TEST(ParallelDownloadUtilsTest, FinchConfigDisabledWithParameter) {
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {content::kMinSliceSizeFinchKey, "4321"},
        {content::kEnableParallelDownloadFinchKey, "false"}};
    feature_list.InitAndEnableFeatureWithParameters(
        features::kParallelDownloading, params);
    // Use |enable_parallel_download| to disable parallel download in enabled
    // experiment group.
    EXPECT_FALSE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {content::kMinSliceSizeFinchKey, "4321"},
        {content::kEnableParallelDownloadFinchKey, "true"}};
    feature_list.InitAndEnableFeatureWithParameters(
        features::kParallelDownloading, params);
    // Disable only if |enable_parallel_download| sets to false.
    EXPECT_TRUE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {content::kMinSliceSizeFinchKey, "4321"}};
    feature_list.InitAndEnableFeatureWithParameters(
        features::kParallelDownloading, params);
    // Empty |enable_parallel_download| in an enabled experiment group will have
    // no impact.
    EXPECT_TRUE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
}

}  // namespace content
