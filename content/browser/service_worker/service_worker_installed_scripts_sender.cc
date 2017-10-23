// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_installed_scripts_sender.h"

#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/net_adapters.h"

namespace content {

namespace {

class MetaDataSender {
 public:
  enum class Status { kSuccess, kFailed };

  MetaDataSender(scoped_refptr<net::IOBufferWithSize> meta_data,
                 mojo::ScopedDataPipeProducerHandle handle)
      : meta_data_(std::move(meta_data)),
        bytes_sent_(0),
        handle_(std::move(handle)),
        watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC),
        weak_factory_(this) {}

  void Start(base::OnceCallback<void(Status)> callback) {
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
        OnCompleted(Status::kFailed);
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        return;
      case MOJO_RESULT_OK:
        break;
      default:
        // mojo::WriteDataRaw() should not return any other values.
        OnCompleted(Status::kFailed);
        return;
    }
    bytes_sent_ += size;
    if (meta_data_->size() == bytes_sent_)
      OnCompleted(Status::kSuccess);
  }

  void OnCompleted(Status status) {
    watcher_.Cancel();
    handle_.reset();
    std::move(callback_).Run(status);
  }

 private:
  base::OnceCallback<void(Status)> callback_;

  scoped_refptr<net::IOBufferWithSize> meta_data_;
  int64_t bytes_sent_;
  mojo::ScopedDataPipeProducerHandle handle_;
  mojo::SimpleWatcher watcher_;

  base::WeakPtrFactory<MetaDataSender> weak_factory_;
};

}  // namespace

// Sender sends a single script to the renderer and calls
// ServiceWorkerIsntalledScriptsSender::OnFinishSendingScript() when done.
class ServiceWorkerInstalledScriptsSender::Sender {
 public:
  Sender(std::unique_ptr<ServiceWorkerResponseReader> reader,
         ServiceWorkerInstalledScriptsSender* owner)
      : reader_(std::move(reader)),
        owner_(owner),
        body_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        weak_factory_(this) {}

  void Start() {
    auto info_buf = base::MakeRefCounted<HttpResponseInfoIOBuffer>();
    reader_->ReadInfo(info_buf.get(), base::Bind(&Sender::OnReadInfoComplete,
                                                 AsWeakPtr(), info_buf));
  }

 private:
  void OnReadInfoComplete(scoped_refptr<HttpResponseInfoIOBuffer> http_info,
                          int result) {
    DCHECK(owner_);
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
      meta_data_sender_ = base::MakeUnique<MetaDataSender>(
          http_info->http_info->metadata, std::move(meta_data_producer));
      meta_data_sender_->Start(
          base::BindOnce(&Sender::OnMetaDataSent, AsWeakPtr()));
      DCHECK_GE(http_info->http_info->metadata->size(), 0);
      meta_data_size = http_info->http_info->metadata->size();
    }

    // Start sending body.
    body_watcher_.Watch(body_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                        base::Bind(&Sender::OnWritableBody, AsWeakPtr()));
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

    owner_->SendScriptInfoToRenderer(
        charset, std::move(header_strings), std::move(body_consumer), body_size,
        std::move(meta_data_consumer), meta_data_size);
    owner_->OnHttpInfoRead(http_info);
  }

  void OnWritableBody(MojoResult) {
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
    reader_->ReadData(buffer.get(), num_bytes,
                      base::Bind(&Sender::OnResponseDataRead, AsWeakPtr()));
  }

  void OnResponseDataRead(int read_bytes) {
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
    ServiceWorkerMetrics::CountReadResponseResult(
        ServiceWorkerMetrics::READ_OK);
    if (read_bytes == 0) {
      // All data has been read.
      body_watcher_.Cancel();
      body_handle_.reset();
      CompleteSendIfNeeded(FinishedReason::kSuccess);
      return;
    }
    body_watcher_.ArmOrNotify();
  }

  void OnMetaDataSent(MetaDataSender::Status status) {
    meta_data_sender_.reset();
    if (status != MetaDataSender::Status::kSuccess) {
      body_watcher_.Cancel();
      body_handle_.reset();
      CompleteSendIfNeeded(FinishedReason::kMetaDataSenderError);
      return;
    }

    CompleteSendIfNeeded(FinishedReason::kSuccess);
  }

  // CompleteSendIfNeeded notifies the end of data transfer to |owner_|, and
  // |this| will be removed by |owner_| as a result. Errors are notified
  // immediately, but when the transfer has been succeeded, it's notified when
  // sending both of body and meta data is finished.
  void CompleteSendIfNeeded(FinishedReason reason) {
    if (reason != FinishedReason::kSuccess) {
      owner_->OnFinishSendingScript(reason);
      return;
    }

    if (WasMetadataWritten() && WasBodyWritten())
      owner_->OnFinishSendingScript(reason);
  }

  bool WasMetadataWritten() const { return !meta_data_sender_; }

  bool WasBodyWritten() const {
    return !body_handle_.is_valid() && !body_pending_write_;
  }

  base::WeakPtr<Sender> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  std::unique_ptr<ServiceWorkerResponseReader> reader_;
  ServiceWorkerInstalledScriptsSender* owner_;

  // For meta data.
  std::unique_ptr<MetaDataSender> meta_data_sender_;

  // For body.
  // Either |body_handle_| or |body_pending_write_| is valid during body is
  // streamed.
  mojo::ScopedDataPipeProducerHandle body_handle_;
  scoped_refptr<network::NetToMojoPendingBuffer> body_pending_write_;
  mojo::SimpleWatcher body_watcher_;

  base::WeakPtrFactory<Sender> weak_factory_;
};

ServiceWorkerInstalledScriptsSender::ServiceWorkerInstalledScriptsSender(
    ServiceWorkerVersion* owner)
    : owner_(owner),
      main_script_url_(owner_->script_url()),
      main_script_id_(
          owner_->script_cache_map()->LookupResourceId(main_script_url_)),
      sent_main_script_(false),
      binding_(this),
      state_(State::kNotStarted),
      last_finished_reason_(FinishedReason::kNotFinished) {
  DCHECK(ServiceWorkerVersion::IsInstalled(owner_->status()));
  DCHECK_NE(kInvalidServiceWorkerResourceId, main_script_id_);
}

ServiceWorkerInstalledScriptsSender::~ServiceWorkerInstalledScriptsSender() {}

mojom::ServiceWorkerInstalledScriptsInfoPtr
ServiceWorkerInstalledScriptsSender::CreateInfoAndBind() {
  DCHECK_EQ(State::kNotStarted, state_);

  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  owner_->script_cache_map()->GetResources(&resources);
  std::vector<GURL> installed_urls;
  for (const auto& resource : resources) {
    installed_urls.emplace_back(resource.url);
    if (resource.url == main_script_url_)
      continue;
    pending_scripts_.emplace(resource.resource_id, resource.url);
  }
  DCHECK(!installed_urls.empty())
      << "At least the main script should be installed.";

  auto info = mojom::ServiceWorkerInstalledScriptsInfo::New();
  info->manager_request = mojo::MakeRequest(&manager_);
  info->installed_urls = std::move(installed_urls);
  binding_.Bind(mojo::MakeRequest(&info->manager_host_ptr));
  return info;
}

void ServiceWorkerInstalledScriptsSender::Start() {
  DCHECK_EQ(State::kNotStarted, state_);
  DCHECK_NE(kInvalidServiceWorkerResourceId, main_script_id_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerInstalledScriptsSender", this,
                                    "main_script_url", main_script_url_.spec());
  StartSendingScript(main_script_id_, main_script_url_);
}

void ServiceWorkerInstalledScriptsSender::StartSendingScript(
    int64_t resource_id,
    const GURL& script_url) {
  DCHECK(!running_sender_);
  DCHECK(current_sending_url_.is_empty());
  state_ = State::kSendingScripts;
  current_sending_url_ = script_url;

  auto reader = owner_->context()->storage()->CreateResponseReader(resource_id);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker", "SendingScript", this,
                                    "script_url", current_sending_url_.spec());
  running_sender_ = base::MakeUnique<Sender>(std::move(reader), this);
  running_sender_->Start();
}

void ServiceWorkerInstalledScriptsSender::SendScriptInfoToRenderer(
    std::string encoding,
    std::unordered_map<std::string, std::string> headers,
    mojo::ScopedDataPipeConsumerHandle body_handle,
    uint64_t body_size,
    mojo::ScopedDataPipeConsumerHandle meta_data_handle,
    uint64_t meta_data_size) {
  DCHECK(running_sender_);
  DCHECK_EQ(State::kSendingScripts, state_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT2(
      "ServiceWorker", "SendScriptInfoToRenderer", this, "body_size", body_size,
      "meta_data_size", meta_data_size);
  auto script_info = mojom::ServiceWorkerScriptInfo::New();
  script_info->script_url = current_sending_url_;
  script_info->headers = std::move(headers);
  script_info->encoding = std::move(encoding);
  script_info->body = std::move(body_handle);
  script_info->body_size = body_size;
  script_info->meta_data = std::move(meta_data_handle);
  script_info->meta_data_size = meta_data_size;
  manager_->TransferInstalledScript(std::move(script_info));
}

void ServiceWorkerInstalledScriptsSender::OnHttpInfoRead(
    scoped_refptr<HttpResponseInfoIOBuffer> http_info) {
  DCHECK(running_sender_);
  DCHECK_EQ(State::kSendingScripts, state_);
  if (IsSendingMainScript())
    owner_->SetMainScriptHttpResponseInfo(*http_info->http_info);
}

void ServiceWorkerInstalledScriptsSender::OnFinishSendingScript(
    FinishedReason reason) {
  DCHECK(running_sender_);
  DCHECK_EQ(State::kSendingScripts, state_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "SendingScript", this);
  running_sender_.reset();
  current_sending_url_ = GURL();

  if (IsSendingMainScript())
    sent_main_script_ = true;

  if (reason != FinishedReason::kSuccess) {
    Abort(reason);
    return;
  }

  if (pending_scripts_.empty()) {
    UpdateFinishedReasonAndBecomeIdle(FinishedReason::kSuccess);
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "ServiceWorker", "ServiceWorkerInstalledScriptsSender", this);
    return;
  }

  // Start sending the next script.
  int64_t next_id = pending_scripts_.front().first;
  GURL next_url = pending_scripts_.front().second;
  pending_scripts_.pop();
  StartSendingScript(next_id, next_url);
}

void ServiceWorkerInstalledScriptsSender::Abort(FinishedReason reason) {
  DCHECK_EQ(State::kSendingScripts, state_);
  DCHECK_NE(FinishedReason::kSuccess, reason);
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker",
                                  "ServiceWorkerInstalledScriptsSender", this,
                                  "FinishedReason", static_cast<int>(reason));

  // Remove all pending scripts.
  // Note that base::queue doesn't have clear(), and also base::STLClearObject
  // is not applicable for base::queue since it doesn't have reserve().
  base::queue<std::pair<int64_t, GURL>> empty;
  pending_scripts_.swap(empty);

  UpdateFinishedReasonAndBecomeIdle(reason);

  switch (reason) {
    case FinishedReason::kNotFinished:
    case FinishedReason::kSuccess:
      NOTREACHED();
      return;
    case FinishedReason::kNoHttpInfoError:
    case FinishedReason::kResponseReaderError:
      owner_->SetStartWorkerStatusCode(SERVICE_WORKER_ERROR_DISK_CACHE);
      // Abort the worker by deleting from the registration since the data was
      // corrupted.
      if (owner_->context()) {
        ServiceWorkerRegistration* registration =
            owner_->context()->GetLiveRegistration(owner_->registration_id());
        // This ends up with destructing |this|.
        registration->DeleteVersion(owner_);
      }
      return;
    case FinishedReason::kCreateDataPipeError:
    case FinishedReason::kConnectionError:
    case FinishedReason::kMetaDataSenderError:
      // Notify the renderer that a connection failure happened. Usually the
      // failure means the renderer gets killed, and the error handler of
      // EmbeddedWorkerInstance is invoked soon.
      manager_.reset();
      binding_.Close();
      return;
  }
}

void ServiceWorkerInstalledScriptsSender::UpdateFinishedReasonAndBecomeIdle(
    FinishedReason reason) {
  DCHECK_EQ(State::kSendingScripts, state_);
  DCHECK_NE(FinishedReason::kNotFinished, reason);
  DCHECK(current_sending_url_.is_empty());
  state_ = State::kIdle;
  last_finished_reason_ = reason;
}

void ServiceWorkerInstalledScriptsSender::RequestInstalledScript(
    const GURL& script_url) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerInstalledScriptsSender::RequestInstalledScript",
               "script_url", script_url.spec());
  int64_t resource_id =
      owner_->script_cache_map()->LookupResourceId(script_url);

  if (resource_id == kInvalidServiceWorkerResourceId) {
    mojo::ReportBadMessage("Requested script was not installed.");
    return;
  }

  if (state_ == State::kSendingScripts) {
    // The sender is now sending other scripts. Push the requested script into
    // the waiting queue.
    pending_scripts_.emplace(resource_id, script_url);
    return;
  }

  DCHECK_EQ(State::kIdle, state_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerInstalledScriptsSender", this,
                                    "main_script_url", main_script_url_.spec());
  StartSendingScript(resource_id, script_url);
}

bool ServiceWorkerInstalledScriptsSender::IsSendingMainScript() const {
  // |current_sending_url_| could match |main_script_url_| even though
  // |sent_main_script_| is false if calling importScripts for the main
  // script.
  return !sent_main_script_ && current_sending_url_ == main_script_url_;
}

}  // namespace content
