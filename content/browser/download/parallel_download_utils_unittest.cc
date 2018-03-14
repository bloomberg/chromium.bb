// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_utils.h"

#include <map>
#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/parallel_download_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ParallelDownloadUtilsTest : public testing::Test {};

// Ensure the minimum slice size is correctly applied.
TEST_F(ParallelDownloadUtilsTest, FindSlicesForRemainingContentMinSliceSize) {
  // Minimum slice size is smaller than total length, only one slice returned.
  download::DownloadItem::ReceivedSlices slices =
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

  // Extreme case where size is smaller than request number.
  slices = FindSlicesForRemainingContent(0, 1, 3, 1);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(download::DownloadItem::ReceivedSlice(0, 0), slices[0]);

  // Normal case.
  slices = FindSlicesForRemainingContent(0, 100, 3, 5);
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(download::DownloadItem::ReceivedSlice(0, 33), slices[0]);
  EXPECT_EQ(download::DownloadItem::ReceivedSlice(33, 33), slices[1]);
  EXPECT_EQ(download::DownloadItem::ReceivedSlice(66, 0), slices[2]);
}

TEST_F(ParallelDownloadUtilsTest, GetMaxContiguousDataBlockSizeFromBeginning) {
  std::vector<download::DownloadItem::ReceivedSlice> slices;
  slices.emplace_back(500, 500);
  EXPECT_EQ(0, GetMaxContiguousDataBlockSizeFromBeginning(slices));

  download::DownloadItem::ReceivedSlice slice1(0, 200);
  download::AddOrMergeReceivedSliceIntoSortedArray(slice1, slices);
  EXPECT_EQ(200, GetMaxContiguousDataBlockSizeFromBeginning(slices));

  download::DownloadItem::ReceivedSlice slice2(200, 300);
  download::AddOrMergeReceivedSliceIntoSortedArray(slice2, slices);
  EXPECT_EQ(1000, GetMaxContiguousDataBlockSizeFromBeginning(slices));
}

// Test to verify Finch parameters for enabled experiment group is read
// correctly.
TEST_F(ParallelDownloadUtilsTest, FinchConfigEnabled) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> params = {
      {content::kMinSliceSizeFinchKey, "1234"},
      {content::kParallelRequestCountFinchKey, "6"},
      {content::kParallelRequestDelayFinchKey, "2000"},
      {content::kParallelRequestRemainingTimeFinchKey, "3"}};
  feature_list.InitAndEnableFeatureWithParameters(
      download::features::kParallelDownloading, params);
  EXPECT_TRUE(IsParallelDownloadEnabled());
  EXPECT_EQ(GetMinSliceSizeConfig(), 1234);
  EXPECT_EQ(GetParallelRequestCountConfig(), 6);
  EXPECT_EQ(GetParallelRequestDelayConfig(), base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(GetParallelRequestRemainingTimeConfig(),
            base::TimeDelta::FromSeconds(3));
}

// Test to verify the disable experiment group will actually disable the
// feature.
TEST_F(ParallelDownloadUtilsTest, FinchConfigDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(download::features::kParallelDownloading);
  EXPECT_FALSE(IsParallelDownloadEnabled());
}

// Test to verify that the Finch parameter |enable_parallel_download| works
// correctly.
TEST_F(ParallelDownloadUtilsTest, FinchConfigDisabledWithParameter) {
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {content::kMinSliceSizeFinchKey, "4321"},
        {content::kEnableParallelDownloadFinchKey, "false"}};
    feature_list.InitAndEnableFeatureWithParameters(
        download::features::kParallelDownloading, params);
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
        download::features::kParallelDownloading, params);
    // Disable only if |enable_parallel_download| sets to false.
    EXPECT_TRUE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {content::kMinSliceSizeFinchKey, "4321"}};
    feature_list.InitAndEnableFeatureWithParameters(
        download::features::kParallelDownloading, params);
    // Empty |enable_parallel_download| in an enabled experiment group will have
    // no impact.
    EXPECT_TRUE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
}

}  // namespace content
