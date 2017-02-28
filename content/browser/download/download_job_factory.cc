// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_job_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_job.h"
#include "content/browser/download/download_job_impl.h"
#include "content/browser/download/parallel_download_job.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {

// Default minimum file size in bytes to enable a parallel download, 2048KB.
// TODO(xingliu): Use finch parameters to configure minimum file size.
const int64_t kMinFileSizeParallelDownload = 2097152;

bool ShouldUseParallelDownload(const DownloadCreateInfo& create_info) {
  // 1. Accept-Ranges, Content-Length and strong validators response headers.
  // 2. Feature |kParallelDownloading| enabled.
  // 3. Content-Length is no less than |kMinFileSizeParallelDownload|.
  // 3. (Undetermined) Http/1.1 protocol.
  // 4. (Undetermined) Not under http proxy, e.g. data saver.

  // Etag and last modified are stored into DownloadCreateInfo in
  // DownloadRequestCore only if the response header complies to the strong
  // validator rule.
  bool has_strong_validator =
      !create_info.etag.empty() || !create_info.last_modified.empty();

  return has_strong_validator && create_info.accept_range &&
         create_info.total_bytes >= kMinFileSizeParallelDownload &&
         base::FeatureList::IsEnabled(features::kParallelDownloading);
}

}  // namespace

std::unique_ptr<DownloadJob> DownloadJobFactory::CreateJob(
    DownloadItemImpl* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> req_handle,
    const DownloadCreateInfo& create_info) {
  // Build parallel download job.
  if (ShouldUseParallelDownload(create_info)) {
    return base::MakeUnique<ParallelDownloadJob>(download_item,
                                                 std::move(req_handle));
  }

  // An ordinary download job.
  return base::MakeUnique<DownloadJobImpl>(download_item,
                                           std::move(req_handle));
}

}  // namespace
