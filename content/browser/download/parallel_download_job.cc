// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_job.h"

#include "base/memory/ptr_util.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/parallel_download_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace content {

ParallelDownloadJob::ParallelDownloadJob(
    DownloadItemImpl* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> request_handle,
    const DownloadCreateInfo& create_info)
    : DownloadJobImpl(download_item, std::move(request_handle)),
      initial_request_offset_(create_info.save_info->offset),
      initial_request_length_(create_info.save_info->length),
      requests_sent_(false) {}

ParallelDownloadJob::~ParallelDownloadJob() = default;

void ParallelDownloadJob::Start() {
  DownloadJobImpl::Start();

  BuildParallelRequestAfterDelay();
}

void ParallelDownloadJob::Cancel(bool user_cancel) {
  DownloadJobImpl::Cancel(user_cancel);

  if (!requests_sent_) {
    timer_.Stop();
    return;
  }

  for (auto& worker : workers_)
    worker->Cancel();
}

void ParallelDownloadJob::Pause() {
  DownloadJobImpl::Pause();

  if (!requests_sent_) {
    timer_.Stop();
    return;
  }

  for (auto& worker : workers_)
    worker->Pause();
}

void ParallelDownloadJob::Resume(bool resume_request) {
  DownloadJobImpl::Resume(resume_request);
  if (!resume_request)
    return;

  // Send parallel requests if the download is paused previously.
  if (!requests_sent_) {
    if (!timer_.IsRunning())
      BuildParallelRequestAfterDelay();
    return;
  }

  for (auto& worker : workers_)
    worker->Resume();
}

void ParallelDownloadJob::ForkRequestsForNewDownload(int64_t bytes_received,
                                                     int64_t total_bytes,
                                                     int request_count) {
  if (!download_item_ || total_bytes <= 0 || bytes_received >= total_bytes ||
      request_count <= 1) {
    return;
  }

  int64_t bytes_left = total_bytes - bytes_received;
  int64_t slice_size = bytes_left / request_count;
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

void ParallelDownloadJob::BuildParallelRequestAfterDelay() {
  DCHECK(workers_.empty());
  DCHECK(!requests_sent_);
  DCHECK(!timer_.IsRunning());

  timer_.Start(FROM_HERE, GetParallelRequestDelayConfig(), this,
               &ParallelDownloadJob::BuildParallelRequests);
}

void ParallelDownloadJob::BuildParallelRequests() {
  DCHECK(!requests_sent_);

  // Calculate the slices to download and fork parallel requests.
  std::vector<DownloadItem::ReceivedSlice> slices_to_download =
      FindSlicesToDownload(download_item_->GetReceivedSlices());
  // The initial request has already been sent, it should cover the first slice.
  DCHECK_GE(slices_to_download[0].offset, initial_request_offset_);
  DCHECK(initial_request_length_ == DownloadSaveInfo::kLengthFullContent ||
         initial_request_offset_ + initial_request_length_ >=
             slices_to_download[0].offset +
             slices_to_download[0].received_bytes);
  if (slices_to_download.size() >=
      static_cast<size_t>(GetParallelRequestCountConfig())) {
    // The size of |slices_to_download| should be no larger than
    // |kParallelRequestCount| unless |kParallelRequestCount| is changed after
    // a download is interrupted. This could happen if we use finch to config
    // the number of parallel requests.
    // TODO(qinmin): Get the next |kParallelRequestCount - 1| slices and fork
    // new requests. For the remaining slices, they will be handled once some
    // of the workers finish their job.
  } else {
    // TODO(qinmin): Check the size of the last slice. If it is huge, we can
    // split it into N pieces and pass the last N-1 pirces to different workers.
    // Otherwise, just fork |slices_to_download.size()| number of workers.
  }

  requests_sent_ = true;
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
