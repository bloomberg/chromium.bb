// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_utils.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {

// Default value for |kMinSliceSizeFinchKey|, when no parameter is specified.
const int64_t kMinSliceSizeParallelDownload = 1365333;

// Default value for |kParallelRequestCountFinchKey|, when no parameter is
// specified.
const int kParallelRequestCount = 3;

// The default remaining download time in seconds required for parallel request
// creation.
const int kDefaultRemainingTimeInSeconds = 2;

// TODO(qinmin): replace this with a comparator operator in
// DownloadItem::ReceivedSlice.
bool compareReceivedSlices(const DownloadItem::ReceivedSlice& lhs,
                           const DownloadItem::ReceivedSlice& rhs) {
  return lhs.offset < rhs.offset;
}

}  // namespace

std::vector<DownloadItem::ReceivedSlice> FindSlicesForRemainingContent(
    int64_t current_offset,
    int64_t total_length,
    int request_count,
    int64_t min_slice_size) {
  std::vector<DownloadItem::ReceivedSlice> new_slices;

  if (request_count > 0) {
    int64_t slice_size =
        std::max<int64_t>(total_length / request_count, min_slice_size);
    slice_size = slice_size > 0 ? slice_size : 1;
    for (int i = 0, num_requests = total_length / slice_size;
         i < num_requests - 1; ++i) {
      new_slices.emplace_back(current_offset, slice_size);
      current_offset += slice_size;
    }
  }

  // Last slice is a half open slice, which results in sending range request
  // like "Range:50-" to fetch from 50 bytes to the end of the file.
  new_slices.emplace_back(current_offset, DownloadSaveInfo::kLengthFullContent);
  return new_slices;
}

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
  return base::StringToInt64(finch_value, &result)
             ? result
             : kMinSliceSizeParallelDownload;
}

int GetParallelRequestCountConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kParallelDownloading, kParallelRequestCountFinchKey);
  int result;
  return base::StringToInt(finch_value, &result) ? result
                                                 : kParallelRequestCount;
}

base::TimeDelta GetParallelRequestDelayConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kParallelDownloading, kParallelRequestDelayFinchKey);
  int64_t time_ms = 0;
  return base::StringToInt64(finch_value, &time_ms)
             ? base::TimeDelta::FromMilliseconds(time_ms)
             : base::TimeDelta::FromMilliseconds(0);
}

base::TimeDelta GetParallelRequestRemainingTimeConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kParallelDownloading, kParallelRequestRemainingTimeFinchKey);
  int time_in_seconds = 0;
  return base::StringToInt(finch_value, &time_in_seconds)
             ? base::TimeDelta::FromSeconds(time_in_seconds)
             : base::TimeDelta::FromSeconds(kDefaultRemainingTimeInSeconds);
}

void DebugSlicesInfo(const DownloadItem::ReceivedSlices& slices) {
  DVLOG(1) << "Received slices size : " << slices.size();
  for (const auto& it : slices) {
    DVLOG(1) << "Slice offset = " << it.offset
             << " , received_bytes = " << it.received_bytes;
  }
}

int64_t GetMaxContiguousDataBlockSizeFromBeginning(
    const DownloadItem::ReceivedSlices& slices) {
  std::vector<DownloadItem::ReceivedSlice>::const_iterator iter =
      slices.begin();

  int64_t size = 0;
  while (iter != slices.end() && iter->offset == size) {
    size += iter->received_bytes;
    iter++;
  }
  return size;
}

bool IsParallelDownloadEnabled() {
  bool feature_enabled =
      base::FeatureList::IsEnabled(features::kParallelDownloading);
  // Disabled when |kEnableParallelDownloadFinchKey| Finch config is set to
  // false.
  bool enabled_parameter = GetFieldTrialParamByFeatureAsBool(
      features::kParallelDownloading, kEnableParallelDownloadFinchKey, true);
  return feature_enabled && enabled_parameter;
}

}  // namespace content
