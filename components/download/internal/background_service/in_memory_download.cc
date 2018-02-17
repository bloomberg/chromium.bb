// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/in_memory_download.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "components/download/internal/background_service/blob_task_proxy.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace download {

namespace {

// Converts a string to HTTP method used by URLFetcher.
net::URLFetcher::RequestType ToRequestType(const std::string& method) {
  // Only supports GET and POST.
  if (base::EqualsCaseInsensitiveASCII(method, "GET"))
    return net::URLFetcher::RequestType::GET;
  if (base::EqualsCaseInsensitiveASCII(method, "POST"))
    return net::URLFetcher::RequestType::POST;

  NOTREACHED();
  return net::URLFetcher::RequestType::GET;
}

}  // namespace

InMemoryDownloadImpl::ResponseWriter::ResponseWriter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : paused_on_io_(false), io_task_runner_(io_task_runner) {}

InMemoryDownloadImpl::ResponseWriter::~ResponseWriter() = default;

void InMemoryDownloadImpl::ResponseWriter::Pause() {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ResponseWriter::PauseOnIO, base::Unretained(this)));
}

void InMemoryDownloadImpl::ResponseWriter::PauseOnIO() {
  io_task_runner_->BelongsToCurrentThread();
  paused_on_io_ = true;
}

void InMemoryDownloadImpl::ResponseWriter::Resume() {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ResponseWriter::ResumeOnIO, base::Unretained(this)));
}

void InMemoryDownloadImpl::ResponseWriter::ResumeOnIO() {
  io_task_runner_->BelongsToCurrentThread();
  paused_on_io_ = false;

  // Continue read from network layer. Since we didn't write on pause, report
  // 0 byte to network layer.
  if (!write_callback_.is_null()) {
    base::ResetAndReturn(&write_callback_).Run(0u);
  }
}

std::unique_ptr<std::string> InMemoryDownloadImpl::ResponseWriter::TakeData() {
  return std::move(data_);
}

int InMemoryDownloadImpl::ResponseWriter::Initialize(
    const net::CompletionCallback& callback) {
  data_ = std::make_unique<std::string>();
  return net::OK;
}

int InMemoryDownloadImpl::ResponseWriter::Write(
    net::IOBuffer* buffer,
    int num_bytes,
    const net::CompletionCallback& callback) {
  io_task_runner_->BelongsToCurrentThread();

  if (paused_on_io_) {
    write_callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  DCHECK(data_);
  data_->append(buffer->data(), num_bytes);
  return num_bytes;
}

int InMemoryDownloadImpl::ResponseWriter::Finish(
    int net_error,
    const net::CompletionCallback& callback) {
  io_task_runner_->BelongsToCurrentThread();
  return net::OK;
}

InMemoryDownload::InMemoryDownload(const std::string& guid)
    : guid_(guid), state_(State::INITIAL), bytes_downloaded_(0u) {}

InMemoryDownload::~InMemoryDownload() = default;

InMemoryDownloadImpl::InMemoryDownloadImpl(
    const std::string& guid,
    const RequestParams& request_params,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Delegate* delegate,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    BlobTaskProxy::BlobContextGetter blob_context_getter,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : InMemoryDownload(guid),
      request_params_(request_params),
      traffic_annotation_(traffic_annotation),
      request_context_getter_(request_context_getter),
      blob_task_proxy_(
          BlobTaskProxy::Create(blob_context_getter, io_task_runner)),
      io_task_runner_(io_task_runner),
      delegate_(delegate),
      paused_(false),
      completion_notified_(false),
      weak_ptr_factory_(this) {
  DCHECK(!guid_.empty());
}

InMemoryDownloadImpl::~InMemoryDownloadImpl() {
  io_task_runner_->DeleteSoon(FROM_HERE, blob_task_proxy_.release());
}

void InMemoryDownloadImpl::Start() {
  DCHECK(state_ == State::INITIAL) << "Only call Start() for new download.";
  SendRequest();
  state_ = State::IN_PROGRESS;
}

void InMemoryDownloadImpl::Pause() {
  if (paused_)
    return;
  paused_ = true;

  switch (state_) {
    case State::INITIAL:
      // Do nothing.
      return;
    case State::IN_PROGRESS:
      // Do nothing if network operation is done.
      DCHECK(response_writer_);
      response_writer_->Pause();
      return;
    case State::FAILED:
      return;
    case State::COMPLETE:
      // Do nothing.
      return;
  }
}

void InMemoryDownloadImpl::Resume() {
  if (!paused_)
    return;
  paused_ = false;

  switch (state_) {
    case State::INITIAL:
      NOTREACHED();
      return;
    case State::IN_PROGRESS:
      // Do nothing if network operation is done.
      DCHECK(response_writer_);
      response_writer_->Resume();
      return;
    case State::FAILED:
      // Restart the network layer. No ongoing blob task should exist.
      SendRequest();
      state_ = State::IN_PROGRESS;
      return;
    case State::COMPLETE:
      NotifyDelegateDownloadComplete();
      return;
  }
}

std::unique_ptr<storage::BlobDataHandle> InMemoryDownloadImpl::ResultAsBlob() {
  DCHECK(state_ == State::COMPLETE || state_ == State::FAILED);
  // Return a copy, we keep one reference of the underlying data to avoid
  // unexpected deletion.
  return std::make_unique<storage::BlobDataHandle>(*blob_data_handle_);
}

size_t InMemoryDownloadImpl::EstimateMemoryUsage() const {
  return bytes_downloaded_;
}

void InMemoryDownloadImpl::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64_t current,
    int64_t total,
    int64_t current_network_bytes) {
  bytes_downloaded_ = current;

  // TODO(xingliu): Throttle the update frequency. See https://crbug.com/809674.
  if (delegate_)
    delegate_->OnDownloadProgress(this);
}

void InMemoryDownloadImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);
  response_headers_ = source->GetResponseHeaders();

  switch (source->GetStatus().status()) {
    case net::URLRequestStatus::Status::SUCCESS:
      if (HandleResponseCode(source->GetResponseCode())) {
        SaveAsBlob();
        return;
      }

      state_ = State::FAILED;
      NotifyDelegateDownloadComplete();
      return;
    case net::URLRequestStatus::Status::IO_PENDING:
      return;
    case net::URLRequestStatus::Status::CANCELED:
    case net::URLRequestStatus::Status::FAILED:
      state_ = State::FAILED;
      NotifyDelegateDownloadComplete();
      return;
  }
}

bool InMemoryDownloadImpl::HandleResponseCode(int response_code) {
  switch (response_code) {
    case -1:  // Non-HTTP request.
    case net::HTTP_OK:
    case net::HTTP_NON_AUTHORITATIVE_INFORMATION:
    case net::HTTP_PARTIAL_CONTENT:
    case net::HTTP_CREATED:
    case net::HTTP_ACCEPTED:
    case net::HTTP_NO_CONTENT:
    case net::HTTP_RESET_CONTENT:
      return true;
    // All other codes are considered as failed.
    default:
      return false;
  }
}

void InMemoryDownloadImpl::SaveAsBlob() {
  DCHECK(url_fetcher_);
  std::unique_ptr<std::string> data = response_writer_->TakeData();
  auto callback = base::BindOnce(&InMemoryDownloadImpl::OnSaveBlobDone,
                                 weak_ptr_factory_.GetWeakPtr());
  blob_task_proxy_->SaveAsBlob(std::move(data), std::move(callback));
}

void InMemoryDownloadImpl::OnSaveBlobDone(
    std::unique_ptr<storage::BlobDataHandle> blob_handle,
    storage::BlobStatus status) {
  // |status| is valid on IO thread, consumer of |blob_handle| should validate
  // the data when using the blob data.
  state_ =
      (status == storage::BlobStatus::DONE) ? State::COMPLETE : State::FAILED;

  // TODO(xingliu): Add metric for blob status code. If failed, consider remove
  // |blob_data_handle_|. See https://crbug.com/809674.
  blob_data_handle_ = std::move(blob_handle);
  completion_time_ = base::Time::Now();

  // Resets network backend.
  response_writer_ = nullptr;
  url_fetcher_.reset();

  // Not considering |paused_| here, if pause after starting a blob operation,
  // just let it finish.
  NotifyDelegateDownloadComplete();
}

void InMemoryDownloadImpl::NotifyDelegateDownloadComplete() {
  if (completion_notified_)
    return;
  completion_notified_ = true;

  if (delegate_)
    delegate_->OnDownloadComplete(this);
}

void InMemoryDownloadImpl::SendRequest() {
  url_fetcher_ = net::URLFetcher::Create(request_params_.url,
                                         ToRequestType(request_params_.method),
                                         this, traffic_annotation_);
  url_fetcher_->SetRequestContext(request_context_getter_.get());
  url_fetcher_->SetExtraRequestHeaders(
      request_params_.request_headers.ToString());
  response_writer_ =
      new ResponseWriter(request_context_getter_->GetNetworkTaskRunner());
  url_fetcher_->SaveResponseWithWriter(
      std::unique_ptr<ResponseWriter>(response_writer_));
  url_fetcher_->Start();

  // Pause on network thread if needed.
  if (paused_)
    response_writer_->Pause();
}

}  // namespace download
