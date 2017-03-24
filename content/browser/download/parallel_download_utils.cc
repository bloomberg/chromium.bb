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

// Finch parameter key value for the delay time in milliseconds to send
// parallel requests after response of the original request is handled.
const char kParallelRequestDelayFinchKey[] = "parallel_request_delay";

// TODO(qinmin): replace this with a comparator operator in
// DownloadItem::ReceivedSlice.
bool compareReceivedSlices(const DownloadItem::ReceivedSlice& lhs,
                           const DownloadItem::ReceivedSlice& rhs) {
  return lhs.offset < rhs.offset;
}

}  // namespace

bool ShouldUseParallelDownload(const DownloadCreateInfo& create_info) {
  // To enable parallel download, following conditions need to be satisfied.
  // 1. Accept-Ranges, Content-Length and strong validators response headers.
  // 2. Feature |kParallelDownloading| enabled.
  // 3. Content-Length is no less than the minimum slice size configuration.
  // 3. HTTP/1.1 protocol, not QUIC nor HTTP/1.0.

  // Etag and last modified are stored into DownloadCreateInfo in
  // DownloadRequestCore only if the response header complies to the strong
  // validator rule.
  bool has_strong_validator =
      !create_info.etag.empty() || !create_info.last_modified.empty();

  return has_strong_validator && create_info.accept_range &&
         create_info.total_bytes >= GetMinSliceSizeConfig() &&
         create_info.connection_info ==
             net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1 &&
         base::FeatureList::IsEnabled(features::kParallelDownloading);
}

std::vector<DownloadItem::ReceivedSlice> FindSlicesForRemainingContent(
    int64_t bytes_received,
    int64_t content_length,
    int request_count) {
  std::vector<DownloadItem::ReceivedSlice> slices_to_download;
  if (request_count <= 0)
    return slices_to_download;

  // TODO(xingliu): Consider to use minimum size of a slice.
  int64_t slice_size = content_length / request_count;
  slice_size = slice_size > 0 ? slice_size : 1;
  int64_t current_offset = bytes_received;
  for (int i = 0, num_requests = content_length / slice_size;
       i < num_requests - 1; ++i) {
    slices_to_download.emplace_back(current_offset, slice_size);
    current_offset += slice_size;
  }

  // Last slice is a half open slice, which results in sending range request
  // like "Range:50-" to fetch from 50 bytes to the end of the file.
  slices_to_download.emplace_back(current_offset,
                                  DownloadSaveInfo::kLengthFullContent);
  return slices_to_download;
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

void DebugSlicesInfo(const DownloadItem::ReceivedSlices& slices) {
  DVLOG(1) << "Received slices size : " << slices.size();
  for (const auto& it : slices) {
    DVLOG(1) << "Slice offset = " << it.offset
             << " , received_bytes = " << it.received_bytes;
  }
}

}  // namespace content
