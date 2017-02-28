// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_worker.h"

#include "content/browser/download/download_create_info.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/web_contents.h"

namespace content {
namespace {

const int kVerboseLevel = 1;

std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>
CreateUrlDownloader(std::unique_ptr<DownloadUrlParameters> params,
                    base::WeakPtr<UrlDownloader::Delegate> delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Build the URLRequest, BlobDataHandle is hold in original request for image
  // download.
  std::unique_ptr<net::URLRequest> url_request =
      DownloadRequestCore::CreateRequestOnIOThread(DownloadItem::kInvalidId,
                                                   params.get());

  return std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>(
      UrlDownloader::BeginDownload(delegate, std::move(url_request),
                                   params->referrer())
          .release());
}

}  // namespace

DownloadWorker::DownloadWorker() : weak_factory_(this) {}

DownloadWorker::~DownloadWorker() = default;

void DownloadWorker::SendRequest(
    std::unique_ptr<DownloadUrlParameters> params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateUrlDownloader, base::Passed(&params),
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DownloadWorker::AddUrlDownloader,
                 weak_factory_.GetWeakPtr()));
}

void DownloadWorker::Pause() {
  request_handle_->PauseRequest();
}

void DownloadWorker::Resume() {
  request_handle_->ResumeRequest();
}

void DownloadWorker::Cancel() {
  request_handle_->CancelRequest();
}

void DownloadWorker::OnUrlDownloaderStarted(
    std::unique_ptr<DownloadCreateInfo> create_info,
    std::unique_ptr<ByteStreamReader> stream_reader,
    const DownloadUrlParameters::OnStartedCallback& callback) {
  // |callback| is not used in subsequent requests.
  DCHECK(callback.is_null());

  // TODO(xingliu): Pass the |stream_reader| to parallel job and handle failed
  // request.
  if (create_info->result !=
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE) {
    VLOG(kVerboseLevel) << "Parallel download sub request failed. reason = "
                        << create_info->result;
    NOTIMPLEMENTED();
    return;
  }

  request_handle_ = std::move(create_info->request_handle);
}

void DownloadWorker::OnUrlDownloaderStopped(UrlDownloader* downloader) {
  // Release the |url_downloader_|, the object will be deleted on IO thread.
  url_downloader_.reset();
}

void DownloadWorker::AddUrlDownloader(
    std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>
        downloader) {
  url_downloader_ = std::move(downloader);
}

}  // namespace content
