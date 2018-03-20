// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_worker.h"

#include "base/message_loop/message_loop.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/input_stream.h"
#include "components/download/public/common/resource_downloader.h"
#include "content/browser/download/download_utils.h"
#include "content/browser/download/url_downloader.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {
namespace {

const int kWorkerVerboseLevel = 1;

class CompletedInputStream : public download::InputStream {
 public:
  CompletedInputStream(download::DownloadInterruptReason status)
      : status_(status){};
  ~CompletedInputStream() override = default;

  // download::InputStream
  bool IsEmpty() override { return false; }
  download::InputStream::StreamState Read(scoped_refptr<net::IOBuffer>* data,
                                          size_t* length) override {
    *length = 0;
    return InputStream::StreamState::COMPLETE;
  }

  download::DownloadInterruptReason GetCompletionStatus() override {
    return status_;
  }

 private:
  download::DownloadInterruptReason status_;
  DISALLOW_COPY_AND_ASSIGN(CompletedInputStream);
};

void CreateUrlDownloadHandler(
    std::unique_ptr<download::DownloadUrlParameters> params,
    base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  download::UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader(
      nullptr, base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    std::unique_ptr<network::ResourceRequest> request =
        CreateResourceRequest(params.get());
    downloader.reset(download::ResourceDownloader::BeginDownload(
                         delegate, std::move(params), std::move(request),
                         url_loader_factory_getter->GetNetworkFactory(), GURL(),
                         GURL(), GURL(), download::DownloadItem::kInvalidId,
                         true, task_runner)
                         .release());
  } else {
    // Build the URLRequest, BlobDataHandle is hold in original request for
    // image download.
    std::unique_ptr<net::URLRequest> url_request =
        DownloadRequestCore::CreateRequestOnIOThread(
            download::DownloadItem::kInvalidId, params.get());

    downloader.reset(UrlDownloader::BeginDownload(
                         delegate, std::move(url_request), params.get(), true)
                         .release());
  }
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &download::UrlDownloadHandler::Delegate::OnUrlDownloadHandlerCreated,
          delegate, std::move(downloader)));
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
      url_download_handler_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      weak_factory_(this) {
  DCHECK(delegate_);
}

DownloadWorker::~DownloadWorker() = default;

void DownloadWorker::SendRequest(
    std::unique_ptr<download::DownloadUrlParameters> params,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter) {
  download::GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateUrlDownloadHandler, std::move(params),
                     weak_factory_.GetWeakPtr(), url_loader_factory_getter,
                     base::ThreadTaskRunnerHandle::Get()));
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
    std::unique_ptr<download::InputStream> input_stream,
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
    input_stream.reset(new CompletedInputStream(create_info->result));
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

void DownloadWorker::OnUrlDownloadStopped(
    download::UrlDownloadHandler* downloader) {
  // Release the |url_download_handler_|, the object will be deleted on IO
  // thread.
  url_download_handler_.reset();
}

void DownloadWorker::OnUrlDownloadHandlerCreated(
    download::UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader) {
  url_download_handler_ = std::move(downloader);
}

}  // namespace content
