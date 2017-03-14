// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_utils.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {

// Finch parameter key value for minimum slice size in bytes to use parallel
// download.
const char kMinSliceSizeFinchKey[] = "min_slice_size";

// Default value for |kMinSliceSizeFinchKey|, when no parameter is specified.
const int64_t kMinSliceSizeParallelDownload = 2097152;

// Finch parameter key value for number of parallel requests in a parallel
// download, including the original request.
const char kParallelRequestCountFinchKey[] = "request_count";

// Default value for |kParallelRequestCountFinchKey|, when no parameter is
// specified.
const int kParallelRequestCount = 2;

// TODO(qinmin): replace this with a comparator operator in
// DownloadItem::ReceivedSlice.
bool compareReceivedSlices(const DownloadItem::ReceivedSlice& lhs,
                           const DownloadItem::ReceivedSlice& rhs) {
  return lhs.offset < rhs.offset;
}

}  // namespace

std::vector<DownloadItem::ReceivedSlice> FindSlicesToDownload(
    const std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  std::vector<DownloadItem::ReceivedSlice> result;
  if (received_slices.empty()) {
    result.emplace_back(0, DownloadSaveInfo::kLengthFullContent);
    return result;
  }

  std::vector<DownloadItem::ReceivedSlice>::const_iterator iter =
      received_slices.begin();
  DCHECK_GE(iter->offset, 0);
  if (iter->offset != 0)
    result.emplace_back(0, iter->offset);

  while (true) {
    int64_t offset = iter->offset + iter->received_bytes;
    std::vector<DownloadItem::ReceivedSlice>::const_iterator next =
        std::next(iter);
    if (next == received_slices.end()) {
      result.emplace_back(offset, DownloadSaveInfo::kLengthFullContent);
      break;
    }

    DCHECK_GE(next->offset, offset);
    if (next->offset > offset)
      result.emplace_back(offset, next->offset - offset);
    iter = next;
  }
  return result;
}

size_t AddOrMergeReceivedSliceIntoSortedArray(
    const DownloadItem::ReceivedSlice& new_slice,
    std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  std::vector<DownloadItem::ReceivedSlice>::iterator it =
      std::upper_bound(received_slices.begin(), received_slices.end(),
                       new_slice, compareReceivedSlices);
  if (it != received_slices.begin()) {
    std::vector<DownloadItem::ReceivedSlice>::iterator prev = std::prev(it);
    if (prev->offset + prev->received_bytes == new_slice.offset) {
      prev->received_bytes += new_slice.received_bytes;
      return static_cast<size_t>(std::distance(received_slices.begin(), prev));
    }
  }

  it = received_slices.emplace(it, new_slice);
  return static_cast<size_t>(std::distance(received_slices.begin(), it));
}

int64_t GetMinSliceSizeConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kParallelDownloading, kMinSliceSizeFinchKey);
  int64_t result;
  return !finch_value.empty() && base::StringToInt64(finch_value, &result)
             ? result
             : kMinSliceSizeParallelDownload;
}

int GetParallelRequestCountConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kParallelDownloading, kParallelRequestCountFinchKey);
  int result;
  return !finch_value.empty() && base::StringToInt(finch_value, &result)
             ? result
             : kParallelRequestCount;
}

}  // namespace content
