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
      initial_request_offset_(create_info.offset),
      content_length_(create_info.total_bytes),
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
    worker.second->Cancel();
}

void ParallelDownloadJob::Pause() {
  DownloadJobImpl::Pause();

  if (!requests_sent_) {
    timer_.Stop();
    return;
  }

  for (auto& worker : workers_)
    worker.second->Pause();
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
    worker.second->Resume();
}

int ParallelDownloadJob::GetParallelRequestCount() const {
  return GetParallelRequestCountConfig();
}

void ParallelDownloadJob::BuildParallelRequestAfterDelay() {
  DCHECK(workers_.empty());
  DCHECK(!requests_sent_);
  DCHECK(!timer_.IsRunning());

  timer_.Start(FROM_HERE, GetParallelRequestDelayConfig(), this,
               &ParallelDownloadJob::BuildParallelRequests);
}

void ParallelDownloadJob::OnByteStreamReady(
    DownloadWorker* worker,
    std::unique_ptr<ByteStreamReader> stream_reader) {
  DownloadJob::AddByteStream(std::move(stream_reader), worker->offset(),
                             worker->length());
}

void ParallelDownloadJob::OnServerResponseError(
    DownloadWorker* worker,
    DownloadInterruptReason reason) {
  // TODO(xingliu): Consider to let the original request to cover the full
  // content if the sub-requests get invalid response. Consider retry on certain
  // error.
  DownloadJob::Interrupt(reason);
}

void ParallelDownloadJob::BuildParallelRequests() {
  DCHECK(!requests_sent_);
  // TODO(qinmin): The size of |slices_to_download| should be no larger than
  // |kParallelRequestCount| unless |kParallelRequestCount| is changed after
  // a download is interrupted. This could happen if we use finch to config
  // the number of parallel requests.
  // Get the next |kParallelRequestCount - 1| slices and fork
  // new requests. For the remaining slices, they will be handled once some
  // of the workers finish their job.
  DownloadItem::ReceivedSlices slices_to_download;
  if (download_item_->GetReceivedSlices().empty()) {
    slices_to_download = FindSlicesForRemainingContent(
        initial_request_offset_, content_length_, GetParallelRequestCount());
  } else {
    // TODO(qinmin): Check the size of the last slice. If it is huge, we can
    // split it into N pieces and pass the last N-1 pieces to different workers.
    // Otherwise, just fork |slices_to_download.size()| number of workers.
    slices_to_download =
        FindSlicesToDownload(download_item_->GetReceivedSlices());
  }

  if (slices_to_download.empty())
    return;

  DCHECK_EQ(slices_to_download[0].offset, initial_request_offset_);
  DCHECK_EQ(slices_to_download.back().received_bytes,
            DownloadSaveInfo::kLengthFullContent);

  // Send requests, does not including the original request.
  ForkSubRequests(slices_to_download);

  requests_sent_ = true;
}

void ParallelDownloadJob::ForkSubRequests(
    const DownloadItem::ReceivedSlices& slices_to_download) {
  if (slices_to_download.size() < 2)
    return;

  for (auto it = slices_to_download.begin() + 1; it != slices_to_download.end();
       ++it) {
    // received_bytes here is the bytes need to download.
    CreateRequest(it->offset, it->received_bytes);
  }
}

void ParallelDownloadJob::CreateRequest(int64_t offset, int64_t length) {
  DCHECK(download_item_);

  std::unique_ptr<DownloadWorker> worker =
      base::MakeUnique<DownloadWorker>(this, offset, length);

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
  DCHECK(workers_.find(offset) == workers_.end());
  workers_[offset] = std::move(worker);
}

}  // namespace content
