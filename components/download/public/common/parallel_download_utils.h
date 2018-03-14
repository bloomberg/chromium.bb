// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_PARALLEL_DOWNLOAD_UTILS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_PARALLEL_DOWNLOAD_UTILS_H_

#include <vector>

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_item.h"

// TODO(qinmin): move all the code here to parallel_download_utils_impl.h once
// parallel download code is moved to components/download.
namespace download {

// Given an array of slices that are received, returns an array of slices to
// download. |received_slices| must be ordered by offsets.
COMPONENTS_DOWNLOAD_EXPORT std::vector<DownloadItem::ReceivedSlice>
FindSlicesToDownload(
    const std::vector<DownloadItem::ReceivedSlice>& received_slices);

// Adds or merges a new received slice into a vector of sorted slices. If the
// slice can be merged with the slice preceding it, merge the 2 slices.
// Otherwise, insert the slice and keep the vector sorted. Returns the index
// of the newly updated slice.
COMPONENTS_DOWNLOAD_EXPORT size_t AddOrMergeReceivedSliceIntoSortedArray(
    const DownloadItem::ReceivedSlice& new_slice,
    std::vector<DownloadItem::ReceivedSlice>& received_slices);

// Returns if a preceding stream can still download the part of content that
// was arranged to |error_stream|.
COMPONENTS_DOWNLOAD_EXPORT bool CanRecoverFromError(
    const DownloadFileImpl::SourceStream* error_stream,
    const DownloadFileImpl::SourceStream* preceding_neighbor);

// Print the states of received slices for debugging.
void DebugSlicesInfo(const DownloadItem::ReceivedSlices& slices);

}  //  namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_PARALLEL_DOWNLOAD_UTILS_H_
