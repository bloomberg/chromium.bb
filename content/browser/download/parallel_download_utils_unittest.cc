// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_utils.h"

#include "content/public/browser/download_save_info.h"
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

}  // namespace content
