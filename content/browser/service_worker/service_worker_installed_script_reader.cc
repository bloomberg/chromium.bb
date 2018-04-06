// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_installed_script_reader.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/net_adapters.h"

namespace content {

class ServiceWorkerInstalledScriptReader::MetaDataSender {
 public:
  MetaDataSender(scoped_refptr<net::IOBufferWithSize> meta_data,
                 mojo::ScopedDataPipeProducerHandle handle)
      : meta_data_(std::move(meta_data)),
        bytes_sent_(0),
        handle_(std::move(handle)),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                 base::SequencedTaskRunnerHandle::Get()),
        weak_factory_(this) {}

  void Start(base::OnceCallback<void(bool /* success */)> callback) {
    callback_ = std::move(callback);
    watcher_.Watch(
        handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        base::Bind(&MetaDataSender::OnWritable, weak_factory_.GetWeakPtr()));
  }

  void OnWritable(MojoResult) {
    // It isn't necessary to handle MojoResult here since WriteDataRaw()
    // returns an equivalent error.
    uint32_t size = meta_data_->size() - bytes_sent_;
    MojoResult rv = handle_->WriteData(meta_data_->data() + bytes_sent_, &size,
                                       MOJO_WRITE_DATA_FLAG_NONE);
    switch (rv) {
      case MOJO_RESULT_INVALID_ARGUMENT:
      case MOJO_RESULT_OUT_OF_RANGE:
      case MOJO_RESULT_BUSY:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        OnCompleted(false);
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        return;
      case MOJO_RESULT_OK:
        break;
      default:
        // mojo::WriteDataRaw() should not return any other values.
        OnCompleted(false);
        return;
    }
    bytes_sent_ += size;
    if (meta_data_->size() == bytes_sent_)
      OnCompleted(true);
  }

  void OnCompleted(bool success) {
    watcher_.Cancel();
    handle_.reset();
    std::move(callback_).Run(success);
  }

 private:
  base::OnceCallback<void(bool /* success */)> callback_;

  scoped_refptr<net::IOBufferWithSize> meta_data_;
  int64_t bytes_sent_;
  mojo::ScopedDataPipeProducerHandle handle_;
  mojo::SimpleWatcher watcher_;

  base::WeakPtrFactory<MetaDataSender> weak_factory_;
};

ServiceWorkerInstalledScriptReader::ServiceWorkerInstalledScriptReader(
    std::unique_ptr<ServiceWorkerResponseReader> reader,
    Client* client)
    : reader_(std::move(reader)),
      client_(client),
      body_watcher_(FROM_HERE,
                    mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                    base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {}

ServiceWorkerInstalledScriptReader::~ServiceWorkerInstalledScriptReader() {}

void ServiceWorkerInstalledScriptReader::Start() {
  auto info_buf = base::MakeRefCounted<HttpResponseInfoIOBuffer>();
  reader_->ReadInfo(
      info_buf.get(),
      base::BindOnce(&ServiceWorkerInstalledScriptReader::OnReadInfoComplete,
                     AsWeakPtr(), info_buf));
}

void ServiceWorkerInstalledScriptReader::OnReadInfoComplete(
    scoped_refptr<HttpResponseInfoIOBuffer> http_info,
    int result) {
  DCHECK(client_);
  DCHECK(http_info);
  if (!http_info->http_info) {
    DCHECK_LT(result, 0);
    ServiceWorkerMetrics::CountReadResponseResult(
        ServiceWorkerMetrics::READ_HEADERS_ERROR);
    CompleteSendIfNeeded(FinishedReason::kNoHttpInfoError);
    return;
  }

  DCHECK_GE(result, 0);
  mojo::ScopedDataPipeConsumerHandle meta_data_consumer;
  mojo::ScopedDataPipeConsumerHandle body_consumer;
  DCHECK_GE(http_info->response_data_size, 0);
  uint64_t body_size = http_info->response_data_size;
  uint64_t meta_data_size = 0;
  if (mojo::CreateDataPipe(nullptr, &body_handle_, &body_consumer) !=
      MOJO_RESULT_OK) {
    CompleteSendIfNeeded(FinishedReason::kCreateDataPipeError);
    return;
  }
  // Start sending meta data (V8 code cache data).
  if (http_info->http_info->metadata) {
    mojo::ScopedDataPipeProducerHandle meta_data_producer;
    if (mojo::CreateDataPipe(nullptr, &meta_data_producer,
                             &meta_data_consumer) != MOJO_RESULT_OK) {
      CompleteSendIfNeeded(FinishedReason::kCreateDataPipeError);
      return;
    }
    meta_data_sender_ = std::make_unique<MetaDataSender>(
        http_info->http_info->metadata, std::move(meta_data_producer));
    meta_data_sender_->Start(base::BindOnce(
        &ServiceWorkerInstalledScriptReader::OnMetaDataSent, AsWeakPtr()));
    DCHECK_GE(http_info->http_info->metadata->size(), 0);
    meta_data_size = http_info->http_info->metadata->size();
  }

  // Start sending body.
  body_watcher_.Watch(
      body_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::Bind(&ServiceWorkerInstalledScriptReader::OnWritableBody,
                 AsWeakPtr()));
  body_watcher_.ArmOrNotify();

  scoped_refptr<net::HttpResponseHeaders> headers =
      http_info->http_info->headers;
  DCHECK(headers);

  std::string charset;
  headers->GetCharset(&charset);

  // Create a map of response headers.
  std::unordered_map<std::string, std::string> header_strings;
  size_t iter = 0;
  std::string key;
  std::string value;
  // This logic is copied from blink::ResourceResponse::AddHTTPHeaderField.
  while (headers->EnumerateHeaderLines(&iter, &key, &value)) {
    if (header_strings.find(key) == header_strings.end()) {
      header_strings[key] = value;
    } else {
      header_strings[key] += ", " + value;
    }
  }

  client_->OnStarted(charset, std::move(header_strings),
                     std::move(body_consumer), body_size,
                     std::move(meta_data_consumer), meta_data_size);
  client_->OnHttpInfoRead(http_info);
}

void ServiceWorkerInstalledScriptReader::OnWritableBody(MojoResult) {
  // It isn't necessary to handle MojoResult here since BeginWrite() returns
  // an equivalent error.
  DCHECK(!body_pending_write_);
  DCHECK(body_handle_.is_valid());
  uint32_t num_bytes = 0;
  MojoResult rv = network::NetToMojoPendingBuffer::BeginWrite(
      &body_handle_, &body_pending_write_, &num_bytes);
  switch (rv) {
    case MOJO_RESULT_INVALID_ARGUMENT:
    case MOJO_RESULT_BUSY:
      NOTREACHED();
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      CompleteSendIfNeeded(FinishedReason::kConnectionError);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      body_watcher_.ArmOrNotify();
      return;
    case MOJO_RESULT_OK:
      // |body_handle_| must have been taken by |body_pending_write_|.
      DCHECK(body_pending_write_);
      DCHECK(!body_handle_.is_valid());
      break;
  }

  scoped_refptr<network::NetToMojoIOBuffer> buffer =
      base::MakeRefCounted<network::NetToMojoIOBuffer>(
          body_pending_write_.get());
  reader_->ReadData(
      buffer.get(), num_bytes,
      base::BindOnce(&ServiceWorkerInstalledScriptReader::OnResponseDataRead,
                     AsWeakPtr()));
}

void ServiceWorkerInstalledScriptReader::OnResponseDataRead(int read_bytes) {
  if (read_bytes < 0) {
    ServiceWorkerMetrics::CountReadResponseResult(
        ServiceWorkerMetrics::READ_DATA_ERROR);
    body_watcher_.Cancel();
    body_handle_.reset();
    CompleteSendIfNeeded(FinishedReason::kResponseReaderError);
    return;
  }
  body_handle_ = body_pending_write_->Complete(read_bytes);
  DCHECK(body_handle_.is_valid());
  body_pending_write_ = nullptr;
  ServiceWorkerMetrics::CountReadResponseResult(ServiceWorkerMetrics::READ_OK);
  if (read_bytes == 0) {
    // All data has been read.
    body_watcher_.Cancel();
    body_handle_.reset();
    CompleteSendIfNeeded(FinishedReason::kSuccess);
    return;
  }
  body_watcher_.ArmOrNotify();
}

void ServiceWorkerInstalledScriptReader::OnMetaDataSent(bool success) {
  meta_data_sender_.reset();
  if (!success) {
    body_watcher_.Cancel();
    body_handle_.reset();
    CompleteSendIfNeeded(FinishedReason::kMetaDataSenderError);
    return;
  }

  CompleteSendIfNeeded(FinishedReason::kSuccess);
}

void ServiceWorkerInstalledScriptReader::CompleteSendIfNeeded(
    FinishedReason reason) {
  if (reason != FinishedReason::kSuccess) {
    client_->OnFinished(reason);
    return;
  }

  if (WasMetadataWritten() && WasBodyWritten())
    client_->OnFinished(reason);
}

};  // namespace content
