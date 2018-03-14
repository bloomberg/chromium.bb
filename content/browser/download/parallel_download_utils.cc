// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_utils.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_save_info.h"

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

}  // namespace

std::vector<download::DownloadItem::ReceivedSlice>
FindSlicesForRemainingContent(int64_t current_offset,
                              int64_t total_length,
                              int request_count,
                              int64_t min_slice_size) {
  std::vector<download::DownloadItem::ReceivedSlice> new_slices;

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

  // No strong assumption that content length header is correct. So the last
  // slice is always half open, which sends range request like "Range:50-".
  new_slices.emplace_back(current_offset,
                          download::DownloadSaveInfo::kLengthFullContent);
  return new_slices;
}

int64_t GetMinSliceSizeConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      download::features::kParallelDownloading, kMinSliceSizeFinchKey);
  int64_t result;
  return base::StringToInt64(finch_value, &result)
             ? result
             : kMinSliceSizeParallelDownload;
}

int GetParallelRequestCountConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      download::features::kParallelDownloading, kParallelRequestCountFinchKey);
  int result;
  return base::StringToInt(finch_value, &result) ? result
                                                 : kParallelRequestCount;
}

base::TimeDelta GetParallelRequestDelayConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      download::features::kParallelDownloading, kParallelRequestDelayFinchKey);
  int64_t time_ms = 0;
  return base::StringToInt64(finch_value, &time_ms)
             ? base::TimeDelta::FromMilliseconds(time_ms)
             : base::TimeDelta::FromMilliseconds(0);
}

base::TimeDelta GetParallelRequestRemainingTimeConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      download::features::kParallelDownloading,
      kParallelRequestRemainingTimeFinchKey);
  int time_in_seconds = 0;
  return base::StringToInt(finch_value, &time_in_seconds)
             ? base::TimeDelta::FromSeconds(time_in_seconds)
             : base::TimeDelta::FromSeconds(kDefaultRemainingTimeInSeconds);
}

int64_t GetMaxContiguousDataBlockSizeFromBeginning(
    const download::DownloadItem::ReceivedSlices& slices) {
  std::vector<download::DownloadItem::ReceivedSlice>::const_iterator iter =
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
      base::FeatureList::IsEnabled(download::features::kParallelDownloading);
  // Disabled when |kEnableParallelDownloadFinchKey| Finch config is set to
  // false.
  bool enabled_parameter = GetFieldTrialParamByFeatureAsBool(
      download::features::kParallelDownloading, kEnableParallelDownloadFinchKey,
      true);
  return feature_enabled && enabled_parameter;
}

}  // namespace content
