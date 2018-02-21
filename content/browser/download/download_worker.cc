// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_worker.h"

#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_utils.h"
#include "content/browser/download/resource_downloader.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/features.h"

namespace content {
namespace {

const int kWorkerVerboseLevel = 1;

class CompletedByteStreamReader : public ByteStreamReader {
 public:
  CompletedByteStreamReader(int status) : status_(status) {};
  ~CompletedByteStreamReader() override = default;

  // ByteStreamReader implementations:
  ByteStreamReader::StreamState Read(scoped_refptr<net::IOBuffer>* data,
                                     size_t* length) override {
    return ByteStreamReader::STREAM_COMPLETE;
  }
  int GetStatus() const override { return status_; }
  void RegisterCallback(const base::Closure& sink_callback) override {}

 private:
  int status_;
};

std::unique_ptr<UrlDownloadHandler, BrowserThread::DeleteOnIOThread>
CreateUrlDownloadHandler(
    std::unique_ptr<download::DownloadUrlParameters> params,
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    std::unique_ptr<network::ResourceRequest> request =
        CreateResourceRequest(params.get());
    return std::unique_ptr<ResourceDownloader, BrowserThread::DeleteOnIOThread>(
        ResourceDownloader::BeginDownload(
            delegate, std::move(params), std::move(request),
            url_loader_factory_getter->GetNetworkFactory(), GURL(), GURL(),
            GURL(), download::DownloadItem::kInvalidId, true)
            .release());
  } else {
    // Build the URLRequest, BlobDataHandle is hold in original request for
    // image download.
    std::unique_ptr<net::URLRequest> url_request =
        DownloadRequestCore::CreateRequestOnIOThread(
            download::DownloadItem::kInvalidId, params.get());

    return std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>(
        UrlDownloader::BeginDownload(delegate, std::move(url_request),
                                     params.get(), true)
            .release());
  }
}

}  // namespace

DownloadWorker::DownloadWorker(DownloadWorker::Delegate* delegate,
                               int64_t offset,
                               int64_t length)
    : delegate_(delegate),
      offset_(offset),
      length_(length),
      is_paused_(false),
      is_canceled_(false),
      is_user_cancel_(false),
      weak_factory_(this) {
  DCHECK(delegate_);
}

DownloadWorker::~DownloadWorker() = default;

void DownloadWorker::SendRequest(
    std::unique_ptr<download::DownloadUrlParameters> params,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&CreateUrlDownloadHandler, std::move(params),
                     weak_factory_.GetWeakPtr(), url_loader_factory_getter),
      base::BindOnce(&DownloadWorker::AddUrlDownloadHandler,
                     weak_factory_.GetWeakPtr()));
}

void DownloadWorker::Pause() {
  is_paused_ = true;
  if (request_handle_)
    request_handle_->PauseRequest();
}

void DownloadWorker::Resume() {
  is_paused_ = false;
  if (request_handle_)
    request_handle_->ResumeRequest();
}

void DownloadWorker::Cancel(bool user_cancel) {
  is_canceled_ = true;
  is_user_cancel_ = user_cancel;
  if (request_handle_)
    request_handle_->CancelRequest(user_cancel);
}

void DownloadWorker::OnUrlDownloadStarted(
    std::unique_ptr<download::DownloadCreateInfo> create_info,
    std::unique_ptr<DownloadManager::InputStream> input_stream,
    const download::DownloadUrlParameters::OnStartedCallback& callback) {
  // |callback| is not used in subsequent requests.
  DCHECK(callback.is_null());

  // Destroy the request if user canceled.
  if (is_canceled_) {
    VLOG(kWorkerVerboseLevel)
        << "Byte stream arrived after user cancel the request.";
    create_info->request_handle->CancelRequest(is_user_cancel_);
    return;
  }

  // TODO(xingliu): Add metric for error handling.
  if (create_info->result != download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    VLOG(kWorkerVerboseLevel)
        << "Parallel download sub-request failed. reason = "
        << create_info->result;
    input_stream->stream_reader_.reset(
        new CompletedByteStreamReader(create_info->result));
  }

  request_handle_ = std::move(create_info->request_handle);

  // Pause the stream if user paused, still push the stream reader to the sink.
  if (is_paused_) {
    VLOG(kWorkerVerboseLevel)
        << "Byte stream arrived after user pause the request.";
    Pause();
  }

  delegate_->OnInputStreamReady(this, std::move(input_stream));
}

void DownloadWorker::OnUrlDownloadStopped(UrlDownloadHandler* downloader) {
  // Release the |url_download_handler_|, the object will be deleted on IO
  // thread.
  url_download_handler_.reset();
}

void DownloadWorker::AddUrlDownloadHandler(
    std::unique_ptr<UrlDownloadHandler, BrowserThread::DeleteOnIOThread>
        downloader) {
  url_download_handler_ = std::move(downloader);
}

}  // namespace content
