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
        watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
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
    watcher_.Watch(body_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                   base::Bind(&Sender::OnWritableBody, AsWeakPtr()));
    watcher_.ArmOrNotify();

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
    DCHECK(!pending_write_);
    DCHECK(body_handle_.is_valid());
    uint32_t num_bytes = 0;
    MojoResult rv = network::NetToMojoPendingBuffer::BeginWrite(
        &body_handle_, &pending_write_, &num_bytes);
    switch (rv) {
      case MOJO_RESULT_INVALID_ARGUMENT:
      case MOJO_RESULT_BUSY:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        CompleteSendIfNeeded(FinishedReason::kConnectionError);
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        watcher_.ArmOrNotify();
        return;
      case MOJO_RESULT_OK:
        // |body_handle_| must have been taken by |pending_write_|.
        DCHECK(pending_write_);
        DCHECK(!body_handle_.is_valid());
        break;
    }

    scoped_refptr<network::NetToMojoIOBuffer> buffer =
        base::MakeRefCounted<network::NetToMojoIOBuffer>(pending_write_.get());
    reader_->ReadData(buffer.get(), num_bytes,
                      base::Bind(&Sender::OnResponseDataRead, AsWeakPtr()));
  }

  void OnResponseDataRead(int read_bytes) {
    if (read_bytes < 0) {
      ServiceWorkerMetrics::CountReadResponseResult(
          ServiceWorkerMetrics::READ_DATA_ERROR);
      watcher_.Cancel();
      body_handle_.reset();
      CompleteSendIfNeeded(FinishedReason::kResponseReaderError);
      return;
    }
    body_handle_ = pending_write_->Complete(read_bytes);
    DCHECK(body_handle_.is_valid());
    pending_write_ = nullptr;
    ServiceWorkerMetrics::CountReadResponseResult(
        ServiceWorkerMetrics::READ_OK);
    if (read_bytes == 0) {
      // All data has been read.
      watcher_.Cancel();
      body_handle_.reset();
      CompleteSendIfNeeded(FinishedReason::kSuccess);
      return;
    }
    watcher_.ArmOrNotify();
  }

  void OnMetaDataSent(MetaDataSender::Status status) {
    meta_data_sender_.reset();
    if (status != MetaDataSender::Status::kSuccess) {
      watcher_.Cancel();
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
  void CompleteSendIfNeeded(FinishedReason status) {
    if (status != FinishedReason::kSuccess) {
      owner_->OnAbortSendingScript(status);
      return;
    }
    if (WasMetadataWritten() && WasBodyWritten())
      owner_->OnFinishSendingScript();
  }

  bool WasMetadataWritten() const { return !meta_data_sender_; }

  bool WasBodyWritten() const {
    return !body_handle_.is_valid() && !pending_write_;
  }

  base::WeakPtr<Sender> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  std::unique_ptr<ServiceWorkerResponseReader> reader_;
  ServiceWorkerInstalledScriptsSender* owner_;

  // For meta data.
  std::unique_ptr<MetaDataSender> meta_data_sender_;

  // For body.
  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  mojo::SimpleWatcher watcher_;

  // Pipes.
  mojo::ScopedDataPipeProducerHandle meta_data_handle_;
  mojo::ScopedDataPipeProducerHandle body_handle_;

  base::WeakPtrFactory<Sender> weak_factory_;
};

ServiceWorkerInstalledScriptsSender::ServiceWorkerInstalledScriptsSender(
    ServiceWorkerVersion* owner)
    : owner_(owner),
      main_script_url_(owner_->script_url()),
      main_script_id_(
          owner_->script_cache_map()->LookupResourceId(main_script_url_)),
      state_(State::kNotStarted),
      finished_reason_(FinishedReason::kNotFinished) {
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
    imported_scripts_.emplace(resource.resource_id, resource.url);
  }
  DCHECK(!installed_urls.empty())
      << "At least the main script should be installed.";

  auto info = mojom::ServiceWorkerInstalledScriptsInfo::New();
  info->manager_request = mojo::MakeRequest(&manager_);
  info->installed_urls = std::move(installed_urls);
  return info;
}

bool ServiceWorkerInstalledScriptsSender::IsFinished() const {
  return state_ == State::kFinished;
}

void ServiceWorkerInstalledScriptsSender::Start() {
  DCHECK_EQ(State::kNotStarted, state_);
  DCHECK_NE(kInvalidServiceWorkerResourceId, main_script_id_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerInstalledScriptsSender", this,
                                    "main_script_url", main_script_url_.spec());
  UpdateState(State::kSendingMainScript);
  StartSendingScript(main_script_id_);
}

void ServiceWorkerInstalledScriptsSender::StartSendingScript(
    int64_t resource_id) {
  DCHECK(!running_sender_);
  DCHECK(state_ == State::kSendingMainScript ||
         state_ == State::kSendingImportedScript);
  auto reader = owner_->context()->storage()->CreateResponseReader(resource_id);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker", "SendingScript", this,
                                    "script_url", CurrentSendingURL().spec());
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
  DCHECK(state_ == State::kSendingMainScript ||
         state_ == State::kSendingImportedScript);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT2(
      "ServiceWorker", "SendScriptInfoToRenderer", this, "body_size", body_size,
      "meta_data_size", meta_data_size);
  auto script_info = mojom::ServiceWorkerScriptInfo::New();
  script_info->script_url = CurrentSendingURL();
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
  DCHECK(state_ == State::kSendingMainScript ||
         state_ == State::kSendingImportedScript);
  if (state_ == State::kSendingMainScript)
    owner_->SetMainScriptHttpResponseInfo(*http_info->http_info);
}

void ServiceWorkerInstalledScriptsSender::OnFinishSendingScript() {
  DCHECK(running_sender_);
  DCHECK(state_ == State::kSendingMainScript ||
         state_ == State::kSendingImportedScript);
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "SendingScript", this);
  running_sender_.reset();
  if (state_ == State::kSendingMainScript) {
    // Imported scripts are served after the main script.
    imported_script_iter_ = imported_scripts_.begin();
    UpdateState(State::kSendingImportedScript);
  } else {
    ++imported_script_iter_;
  }
  if (imported_script_iter_ == imported_scripts_.end()) {
    // All scripts have been sent to the renderer.
    // ServiceWorkerInstalledScriptsSender's work is done now.
    DCHECK_EQ(State::kSendingImportedScript, state_);
    DCHECK(!IsFinished());
    Finish(FinishedReason::kSuccess);
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "ServiceWorker", "ServiceWorkerInstalledScriptsSender", this);
    return;
  }
  // Start sending the next script.
  StartSendingScript(imported_script_iter_->first);
}

void ServiceWorkerInstalledScriptsSender::OnAbortSendingScript(
    FinishedReason reason) {
  DCHECK(running_sender_);
  DCHECK(state_ == State::kSendingMainScript ||
         state_ == State::kSendingImportedScript);
  DCHECK_NE(FinishedReason::kSuccess, reason);
  DCHECK(!IsFinished());
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker",
                                  "ServiceWorkerInstalledScriptsSender", this,
                                  "FinishedReason", static_cast<int>(reason));
  running_sender_.reset();
  Finish(reason);

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
      return;
  }
}

const GURL& ServiceWorkerInstalledScriptsSender::CurrentSendingURL() {
  if (state_ == State::kSendingMainScript)
    return main_script_url_;
  return imported_script_iter_->second;
}

void ServiceWorkerInstalledScriptsSender::UpdateState(State state) {
  DCHECK_NE(State::kFinished, state) << "Use Finish() for state kFinished.";

  switch (state_) {
    case State::kNotStarted:
      DCHECK_EQ(State::kSendingMainScript, state);
      state_ = state;
      return;
    case State::kSendingMainScript:
      DCHECK_EQ(State::kSendingImportedScript, state);
      state_ = state;
      return;
    case State::kSendingImportedScript:
    case State::kFinished:
      NOTREACHED();
      return;
  }
}

void ServiceWorkerInstalledScriptsSender::Finish(FinishedReason reason) {
  DCHECK_NE(State::kFinished, state_);
  DCHECK_NE(FinishedReason::kNotFinished, reason);
  state_ = State::kFinished;
  finished_reason_ = reason;
}

}  // namespace content
