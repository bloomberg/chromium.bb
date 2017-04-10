// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_job_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_job.h"
#include "content/browser/download/download_job_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/download/parallel_download_job.h"
#include "content/browser/download/parallel_download_utils.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {

// Returns if the download should be a parallel download.
bool ShouldUseParallelDownload(const DownloadCreateInfo& create_info) {
  // To enable parallel download, following conditions need to be satisfied.
  // 1. Feature |kParallelDownloading| enabled.
  // 2. Strong validators response headers. i.e. ETag and Last-Modified.
  // 3. Accept-Ranges header.
  // 4. Content-Length header.
  // 5. Content-Length is no less than the minimum slice size configuration.
  // 6. HTTP/1.1 protocol, not QUIC nor HTTP/1.0.
  if (!base::FeatureList::IsEnabled(features::kParallelDownloading))
    return false;

  // Etag and last modified are stored into DownloadCreateInfo in
  // DownloadRequestCore only if the response header complies to the strong
  // validator rule.
  bool has_strong_validator =
      !create_info.etag.empty() || !create_info.last_modified.empty();
  bool has_content_length = create_info.total_bytes > 0;
  bool satisfy_min_file_size =
      create_info.total_bytes >= GetMinSliceSizeConfig();
  bool satisfy_connection_type = create_info.connection_info ==
                                 net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1;

  bool should_use_parallel_download =
      has_strong_validator && create_info.accept_range && has_content_length &&
      satisfy_min_file_size && satisfy_connection_type;

  RecordParallelDownloadCreationEvent(
      should_use_parallel_download
          ? ParallelDownloadCreationEvent::STARTED_PARALLEL_DOWNLOAD
          : ParallelDownloadCreationEvent::FELL_BACK_TO_NORMAL_DOWNLOAD);

  if (!has_strong_validator) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_STRONG_VALIDATORS);
  }
  if (!create_info.accept_range) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_ACCEPT_RANGE_HEADER);
  }
  if (!has_content_length) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_CONTENT_LENGTH_HEADER);
  }
  if (!satisfy_min_file_size) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_FILE_SIZE);
  }
  if (!satisfy_connection_type) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_CONNECTION_TYPE);
  }

  return should_use_parallel_download;
}

}  // namespace

std::unique_ptr<DownloadJob> DownloadJobFactory::CreateJob(
    DownloadItemImpl* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> req_handle,
    const DownloadCreateInfo& create_info) {
  // Build parallel download job.
  if (ShouldUseParallelDownload(create_info)) {
    return base::MakeUnique<ParallelDownloadJob>(download_item,
                                                 std::move(req_handle),
                                                 create_info);
  }

  // An ordinary download job.
  return base::MakeUnique<DownloadJobImpl>(download_item,
                                           std::move(req_handle));
}

}  // namespace
