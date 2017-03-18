// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_
#define CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_

#include <vector>

#include "content/browser/download/download_create_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"

namespace content {

// Return if the download should be a parallel download.
CONTENT_EXPORT bool ShouldUseParallelDownload(
    const DownloadCreateInfo& create_info);

// Return the slices to download for the remaining content.
// This function chunks the content into slices.
CONTENT_EXPORT std::vector<DownloadItem::ReceivedSlice>
FindSlicesForRemainingContent(int64_t bytes_received,
                              int64_t content_length,
                              int request_count);

// Given an array of slices that are received, returns an array of slices to
// download. |received_slices| must be ordered by offsets.
CONTENT_EXPORT std::vector<DownloadItem::ReceivedSlice> FindSlicesToDownload(
    const std::vector<DownloadItem::ReceivedSlice>& received_slices);

// Adds or merges a new received slice into a vector of sorted slices. If the
// slice can be merged with the slice preceding it, merge the 2 slices.
// Otherwise, insert the slice and keep the vector sorted. Returns the index
// of the newly updated slice.
CONTENT_EXPORT size_t AddOrMergeReceivedSliceIntoSortedArray(
    const DownloadItem::ReceivedSlice& new_slice,
    std::vector<DownloadItem::ReceivedSlice>& received_slices);

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

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_
