// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_job.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace content {

namespace {

// TODO(xingliu): Use finch parameters to configure constants.
// Default number of requests in a parallel download, including the original
// request.
const int kParallelRequestCount = 2;

}  // namespace

ParallelDownloadJob::ParallelDownloadJob(
    DownloadItemImpl* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> request_handle)
    : DownloadJobImpl(download_item, std::move(request_handle)),
      request_num_(kParallelRequestCount) {}

ParallelDownloadJob::~ParallelDownloadJob() = default;

void ParallelDownloadJob::Cancel(bool user_cancel) {
  DownloadJobImpl::Cancel(user_cancel);
  for (auto& worker : workers_)
    worker->Cancel();
}

void ParallelDownloadJob::Pause() {
  DownloadJobImpl::Pause();
  for (auto& worker : workers_)
    worker->Pause();
}

void ParallelDownloadJob::Resume(bool resume_request) {
  DownloadJobImpl::Resume(resume_request);
  if (!resume_request)
    return;

  for (auto& worker : workers_)
    worker->Resume();
}

void ParallelDownloadJob::ForkRequestsForNewDownload(int64_t bytes_received,
                                                     int64_t total_bytes) {
  if (!download_item_ || total_bytes <= 0 || bytes_received >= total_bytes ||
      request_num_ <= 1) {
    return;
  }

  int64_t bytes_left = total_bytes - bytes_received;
  int64_t slice_size = bytes_left / request_num_;
  slice_size = slice_size > 0 ? slice_size : 1;
  int num_requests = bytes_left / slice_size;
  int64_t current_offset = bytes_received + slice_size;

  // TODO(xingliu): Add records for slices in history db.
  for (int i = 0; i < num_requests - 1; ++i) {
    int64_t length = (i == (num_requests - 2))
                         ? slice_size + (bytes_left % slice_size)
                         : slice_size;
    CreateRequest(current_offset, length);
    current_offset += slice_size;
  }
}

void ParallelDownloadJob::CreateRequest(int64_t offset, int64_t length) {
  std::unique_ptr<DownloadWorker> worker = base::MakeUnique<DownloadWorker>();

  DCHECK(download_item_);
  StoragePartition* storage_partition =
      BrowserContext::GetStoragePartitionForSite(
          download_item_->GetBrowserContext(), download_item_->GetSiteUrl());

  std::unique_ptr<DownloadUrlParameters> download_params(
      new DownloadUrlParameters(download_item_->GetURL(),
                                storage_partition->GetURLRequestContext()));
  download_params->set_file_path(download_item_->GetFullPath());
  download_params->set_last_modified(download_item_->GetLastModifiedTime());
  download_params->set_etag(download_item_->GetETag());
  download_params->set_offset(offset);

  // Setting the length will result in range request to fetch a slice of the
  // file.
  download_params->set_length(length);

  // Subsequent range requests have the same referrer URL as the original
  // download request.
  download_params->set_referrer(Referrer(download_item_->GetReferrerUrl(),
                                         blink::WebReferrerPolicyAlways));
  // Send the request.
  worker->SendRequest(std::move(download_params));
  workers_.push_back(std::move(worker));
}

}  // namespace content
