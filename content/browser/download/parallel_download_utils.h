// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_
#define CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"

namespace content {

// Find the first gap in an array of received slices and return it as the next
// slice to download. If not found, return a slice that is to the end of all
// slices. |received_slices| must be ordered by offsets.
CONTENT_EXPORT DownloadItem::ReceivedSlice FindNextSliceToDownload(
    const std::vector<DownloadItem::ReceivedSlice>& received_slices);

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_UTILS_H_
