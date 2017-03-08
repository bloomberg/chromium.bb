// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_
#define CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"

namespace content {

// Given an array of slices that are received, returns an array of slices to
// download. |received_slices| must be ordered by offsets.
CONTENT_EXPORT std::vector<DownloadItem::ReceivedSlice> FindSlicesToDownload(
    const std::vector<DownloadItem::ReceivedSlice>& received_slices);

// Finch configuration utilities.
//
// Get the minimum slice size to use parallel download from finch configuration.
// A slice won't be further chunked into smaller slices if the size is less
// than the minimum size.
CONTENT_EXPORT int64_t GetMinSliceSizeConfig();

// Get the request count for parallel download from finch configuration.
CONTENT_EXPORT int GetParallelRequestCountConfig();

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_
