// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_utils.h"

#include "content/public/browser/download_save_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ParallelDownloadUtilsTest, FindNextSliceToDownload) {
  std::vector<DownloadItem::ReceivedSlice> slices;
  DownloadItem::ReceivedSlice slice = FindNextSliceToDownload(slices);
  EXPECT_EQ(0, slice.offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent, slice.received_bytes);

  slices.emplace_back(0, 500);
  slice = FindNextSliceToDownload(slices);
  EXPECT_EQ(500, slice.offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent, slice.received_bytes);

  // Create a gap between slices.
  slices.emplace_back(1000, 500);
  slice = FindNextSliceToDownload(slices);
  EXPECT_EQ(500, slice.offset);
  EXPECT_EQ(500, slice.received_bytes);

  // Fill the gap.
  slices.emplace(slices.begin() + 1, slice);
  slice = FindNextSliceToDownload(slices);
  EXPECT_EQ(1500, slice.offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent, slice.received_bytes);

  // Create a new gap at the beginning.
  slices.erase(slices.begin());
  slice = FindNextSliceToDownload(slices);
  EXPECT_EQ(0, slice.offset);
  EXPECT_EQ(500, slice.received_bytes);
}

}  // namespace content
