// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_
#define CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_

#include <vector>

#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_item.h"
#include "content/common/content_export.h"

namespace content {

// Finch parameter key value to enable parallel download. Used in enabled
// experiment group that needs other parameters, such as min_slice_size, but
// don't want to actually do parallel download.
constexpr char kEnableParallelDownloadFinchKey[] = "enable_parallel_download";

// Finch parameter key value for minimum slice size in bytes to use parallel
// download.
constexpr char kMinSliceSizeFinchKey[] = "min_slice_size";

// Finch parameter key value for number of parallel requests in a parallel
// download, including the original request.
constexpr char kParallelRequestCountFinchKey[] = "request_count";

// Finch parameter key value for the delay time in milliseconds to send
// parallel requests after response of the original request is handled.
constexpr char kParallelRequestDelayFinchKey[] = "parallel_request_delay";

// Finch parameter key value for the remaining time in seconds that is required
// to send parallel requests.
constexpr char kParallelRequestRemainingTimeFinchKey[] =
    "parallel_request_remaining_time";

// Chunks the content that starts from |current_offset|, into at most
// std::max(|request_count|, 1) smaller slices.
// Each slice contains at least |min_slice_size| bytes unless |total_length|
// is less than |min_slice_size|.
// The last slice is half opened.
CONTENT_EXPORT std::vector<download::DownloadItem::ReceivedSlice>
FindSlicesForRemainingContent(int64_t current_offset,
                              int64_t total_length,
                              int request_count,
                              int64_t min_slice_size);

// Finch configuration utilities.
//
// Get the minimum slice size to use parallel download from finch configuration.
// A slice won't be further chunked into smaller slices if the size is less
// than the minimum size.
CONTENT_EXPORT int64_t GetMinSliceSizeConfig();

// Get the request count for parallel download from finch configuration.
CONTENT_EXPORT int GetParallelRequestCountConfig();

// Get the time delay to send parallel requests after the response of original
// request is handled.
CONTENT_EXPORT base::TimeDelta GetParallelRequestDelayConfig();

// Get the required remaining time before creating parallel requests.
CONTENT_EXPORT base::TimeDelta GetParallelRequestRemainingTimeConfig();

// Given an ordered array of slices, get the maximum size of a contiguous data
// block that starts from offset 0. If the first slice doesn't start from offset
// 0, return 0.
CONTENT_EXPORT int64_t GetMaxContiguousDataBlockSizeFromBeginning(
    const download::DownloadItem::ReceivedSlices& slices);

// Returns whether parallel download is enabled.
CONTENT_EXPORT bool IsParallelDownloadEnabled();

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_
