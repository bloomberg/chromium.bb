// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/url_downloader.h"

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_save_info.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/base/page_transition_types.h"

namespace content {

class UrlDownloader::RequestHandle : public DownloadRequestHandleInterface {
 public:
  RequestHandle(base::WeakPtr<UrlDownloader> downloader,
                base::WeakPtr<DownloadManagerImpl> download_manager_impl,
                scoped_refptr<base::SequencedTaskRunner> downloader_task_runner)
      : downloader_(downloader),
        download_manager_impl_(download_manager_impl),
        downloader_task_runner_(downloader_task_runner) {}
  RequestHandle(RequestHandle&& other)
      : downloader_(std::move(other.downloader_)),
        download_manager_impl_(std::move(other.download_manager_impl_)),
        downloader_task_runner_(std::move(other.downloader_task_runner_)) {}
  RequestHandle& operator=(RequestHandle&& other) {
    downloader_ = std::move(other.downloader_);
    download_manager_impl_ = std::move(other.download_manager_impl_);
    downloader_task_runner_ = std::move(other.downloader_task_runner_);
    return *this;
  }

  // DownloadRequestHandleInterface
  WebContents* GetWebContents() const override { return nullptr; }
  DownloadManager* GetDownloadManager() const override {
    return download_manager_impl_ ? download_manager_impl_.get() : nullptr;
  }
  void PauseRequest() const override {
    downloader_task_runner_->PostTask(
        FROM_HERE, base::Bind(&UrlDownloader::PauseRequest, downloader_));
  }
  void ResumeRequest() const override {
    downloader_task_runner_->PostTask(
        FROM_HERE, base::Bind(&UrlDownloader::ResumeRequest, downloader_));
  }
  void CancelRequest() const override {
    downloader_task_runner_->PostTask(
        FROM_HERE, base::Bind(&UrlDownloader::CancelRequest, downloader_));
  }
  std::string DebugString() const override { return std::string(); }

 private:
  base::WeakPtr<UrlDownloader> downloader_;
  base::WeakPtr<DownloadManagerImpl> download_manager_impl_;
  scoped_refptr<base::SequencedTaskRunner> downloader_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RequestHandle);
};

// static
scoped_ptr<UrlDownloader> UrlDownloader::BeginDownload(
    base::WeakPtr<DownloadManagerImpl> download_manager,
    scoped_ptr<net::URLRequest> request,
    const Referrer& referrer,
    bool prefer_cache,
    scoped_ptr<DownloadSaveInfo> save_info,
    uint32_t download_id,
    const DownloadUrlParameters::OnStartedCallback& started_callback) {
  if (!referrer.url.is_valid())
    request->SetReferrer(std::string());
  else
    request->SetReferrer(referrer.url.spec());

  int extra_load_flags = net::LOAD_NORMAL;
  if (prefer_cache) {
    // If there is upload data attached, only retrieve from cache because there
    // is no current mechanism to prompt the user for their consent for a
    // re-post. For GETs, try to retrieve data from the cache and skip
    // validating the entry if present.
    if (request->get_upload() != NULL)
      extra_load_flags |= net::LOAD_ONLY_FROM_CACHE;
    else
      extra_load_flags |= net::LOAD_PREFERRING_CACHE;
  } else {
    extra_load_flags |= net::LOAD_DISABLE_CACHE;
  }
  request->SetLoadFlags(request->load_flags() | extra_load_flags);

  if (request->url().SchemeIs(url::kBlobScheme))
    return nullptr;

  // From this point forward, the |UrlDownloader| is responsible for
  // |started_callback|.
  scoped_ptr<UrlDownloader> downloader(
      new UrlDownloader(std::move(request), download_manager,
                        std::move(save_info), download_id, started_callback));
  downloader->Start();

  return downloader;
}

UrlDownloader::UrlDownloader(
    scoped_ptr<net::URLRequest> request,
    base::WeakPtr<DownloadManagerImpl> manager,
    scoped_ptr<DownloadSaveInfo> save_info,
    uint32_t download_id,
    const DownloadUrlParameters::OnStartedCallback& on_started_callback)
    : request_(std::move(request)),
      manager_(manager),
      download_id_(download_id),
      on_started_callback_(on_started_callback),
      handler_(
          request_.get(),
          std::move(save_info),
          base::Bind(&UrlDownloader::ResumeReading, base::Unretained(this))),
      weak_ptr_factory_(this) {}

UrlDownloader::~UrlDownloader() {
  CallStartedCallbackOnFailure(DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
}

void UrlDownloader::Start() {
  DCHECK(!request_->is_pending());

  if (!request_->status().is_success())
    return;

  request_->set_delegate(this);
  request_->Start();
}

void UrlDownloader::OnReceivedRedirect(net::URLRequest* request,
                                       const net::RedirectInfo& redirect_info,
                                       bool* defer_redirect) {
  DVLOG(1) << "OnReceivedRedirect: " << request_->url().spec();
  request_->CancelWithError(net::ERR_ABORTED);
}

void UrlDownloader::OnResponseStarted(net::URLRequest* request) {
  DVLOG(1) << "OnResponseStarted: " << request_->url().spec();

  if (!request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  scoped_ptr<DownloadCreateInfo> create_info;
  scoped_ptr<ByteStreamReader> stream_reader;

  handler_.OnResponseStarted(&create_info, &stream_reader);

  create_info->download_id = download_id_;
  create_info->request_handle.reset(
      new RequestHandle(weak_ptr_factory_.GetWeakPtr(), manager_,
                        base::SequencedTaskRunnerHandle::Get()));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadManagerImpl::StartDownload, manager_,
                 base::Passed(&create_info), base::Passed(&stream_reader),
                 base::ResetAndReturn(&on_started_callback_)));

  if (request_->status().is_success())
    StartReading(false);  // Read the first chunk.
  else
    ResponseCompleted();
}

void UrlDownloader::StartReading(bool is_continuation) {
  int bytes_read;

  // Make sure we track the buffer in at least one place.  This ensures it gets
  // deleted even in the case the request has already finished its job and
  // doesn't use the buffer.
  scoped_refptr<net::IOBuffer> buf;
  int buf_size;
  if (!handler_.OnWillRead(&buf, &buf_size, -1)) {
    request_->CancelWithError(net::ERR_ABORTED);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&UrlDownloader::ResponseCompleted,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(buf.get());
  DCHECK(buf_size > 0);

  request_->Read(buf.get(), buf_size, &bytes_read);

  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (request_->status().is_io_pending())
    return;

  if (!is_continuation || bytes_read <= 0) {
    OnReadCompleted(request_.get(), bytes_read);
  } else {
    // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
    // thread in case the URLRequest can provide data synchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&UrlDownloader::OnReadCompleted,
                   weak_ptr_factory_.GetWeakPtr(), request_.get(), bytes_read));
  }
}

void UrlDownloader::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  DVLOG(1) << "OnReadCompleted: \"" << request_->url().spec() << "\""
           << " bytes_read = " << bytes_read;

  // bytes_read == -1 always implies an error.
  if (bytes_read == -1 || !request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  DCHECK(bytes_read >= 0);
  DCHECK(request_->status().is_success());

  bool defer = false;
  if (!handler_.OnReadCompleted(bytes_read, &defer)) {
    request_->CancelWithError(net::ERR_ABORTED);
    return;
  } else if (defer) {
    return;
  }

  if (!request_->status().is_success())
    return;

  if (bytes_read > 0) {
    StartReading(true);  // Read the next chunk.
  } else {
    // URLRequest reported an EOF. Call ResponseCompleted.
    DCHECK_EQ(0, bytes_read);
    ResponseCompleted();
  }
}

void UrlDownloader::ResponseCompleted() {
  DVLOG(1) << "ResponseCompleted: " << request_->url().spec();

  handler_.OnResponseCompleted(request_->status());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadManagerImpl::RemoveUrlDownloader, manager_, this));
}

void UrlDownloader::ResumeReading() {
  if (request_->status().is_success()) {
    StartReading(false);  // Read the next chunk (OK to complete synchronously).
  } else {
    ResponseCompleted();
  }
}

void UrlDownloader::CallStartedCallbackOnFailure(
    DownloadInterruptReason result) {
  if (on_started_callback_.is_null())
    return;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(base::ResetAndReturn(&on_started_callback_), nullptr, result));
}

void UrlDownloader::PauseRequest() {
  handler_.PauseRequest();
}

void UrlDownloader::ResumeRequest() {
  handler_.ResumeRequest();
}

void UrlDownloader::CancelRequest() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadManagerImpl::RemoveUrlDownloader, manager_, this));
}

}  // namespace content
