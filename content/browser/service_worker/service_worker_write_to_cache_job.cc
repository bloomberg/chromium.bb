// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_write_to_cache_job.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

namespace content {

namespace {

const char kKilledError[] = "The request to fetch the script was interrupted.";
const char kBadHTTPResponseError[] =
    "A bad HTTP response code (%d) was received when fetching the script.";
const char kSSLError[] =
    "An SSL certificate error occurred when fetching the script.";
const char kBadMIMEError[] = "The script has an unsupported MIME type ('%s').";
const char kNoMIMEError[] = "The script does not have a MIME type.";
const char kClientAuthenticationError[] =
    "Client authentication was required to fetch the script.";
const char kRedirectError[] =
    "The script resource is behind a redirect, which is disallowed.";
const char kServiceWorkerAllowed[] = "Service-Worker-Allowed";

const int kBufferSize = 16 * 1024;

}  // namespace

// Reads an existing resource and copies it via
// ServiceWorkerWriteToCacheJob::WriteData.
class ServiceWorkerWriteToCacheJob::Copier : public base::RefCounted<Copier> {
 public:
  Copier(base::WeakPtr<ServiceWorkerWriteToCacheJob> owner,
         scoped_ptr<ServiceWorkerResponseReader> reader,
         int bytes_to_copy,
         const base::Callback<void(ServiceWorkerStatusCode)>& callback)
      : owner_(owner),
        reader_(reader.release()),
        bytes_to_copy_(bytes_to_copy),
        callback_(callback) {
    DCHECK_LT(0, bytes_to_copy_);
  }

  void Start() {
    io_buffer_ = new net::IOBuffer(kBufferSize);
    ReadSomeData();
  }

 private:
  friend class base::RefCounted<Copier>;
  ~Copier() {}

  void ReadSomeData() {
    reader_->ReadData(io_buffer_.get(), kBufferSize,
                      base::Bind(&Copier::OnReadDataComplete, this));
  }

  void OnReadDataComplete(int result) {
    if (!owner_)
      return;
    if (result <= 0) {
      // We hit EOF or error but weren't done copying.
      Complete(SERVICE_WORKER_ERROR_FAILED);
      return;
    }

    int bytes_to_write = std::min(bytes_to_copy_, result);
    owner_->WriteData(io_buffer_.get(), bytes_to_write,
                      base::Bind(&Copier::OnWriteDataComplete, this));
  }

  void OnWriteDataComplete(int result) {
    if (result < 0) {
      Complete(SERVICE_WORKER_ERROR_FAILED);
      return;
    }

    DCHECK_LE(result, bytes_to_copy_);
    bytes_to_copy_ -= result;
    if (bytes_to_copy_ == 0) {
      Complete(SERVICE_WORKER_OK);
      return;
    }

    ReadSomeData();
  }

  void Complete(ServiceWorkerStatusCode status) {
    if (!owner_)
      return;
    callback_.Run(status);
  }

  base::WeakPtr<ServiceWorkerWriteToCacheJob> owner_;
  scoped_ptr<ServiceWorkerResponseReader> reader_;
  int bytes_to_copy_ = 0;
  base::Callback<void(ServiceWorkerStatusCode)> callback_;
  scoped_refptr<net::IOBuffer> io_buffer_;
};

// Abstract consumer for ServiceWorkerWriteToCacheJob that processes data from
// the network.
class ServiceWorkerWriteToCacheJob::NetDataConsumer {
 public:
  virtual ~NetDataConsumer() {}

  // Called by |owner_|'s OnResponseStarted.
  virtual void OnResponseStarted() = 0;

  // HandleData should call |owner_|->NotifyReadComplete() when done.
  virtual void HandleData(net::IOBuffer* buf, int size) = 0;
};

// Dumb consumer that just writes everything it sees to disk.
class ServiceWorkerWriteToCacheJob::PassThroughConsumer
    : public ServiceWorkerWriteToCacheJob::NetDataConsumer {
 public:
  explicit PassThroughConsumer(ServiceWorkerWriteToCacheJob* owner)
      : owner_(owner), weak_factory_(this) {}
  ~PassThroughConsumer() override {}

  void OnResponseStarted() override {
    owner_->WriteHeaders(
        base::Bind(&PassThroughConsumer::OnWriteHeadersComplete,
                   weak_factory_.GetWeakPtr()));
    owner_->SetPendingIO();
  }

  void HandleData(net::IOBuffer* buf, int size) override {
    if (size == 0) {
      owner_->OnPassThroughComplete();
      return;
    }

    owner_->WriteData(buf, size,
                      base::Bind(&PassThroughConsumer::OnWriteDataComplete,
                                 weak_factory_.GetWeakPtr()));
    owner_->SetPendingIO();
  }

 private:
  void OnWriteHeadersComplete() {
    owner_->ClearPendingIO();
    owner_->CommitHeadersAndNotifyHeadersComplete();
  }

  void OnWriteDataComplete(int result) {
    owner_->ClearPendingIO();
    owner_->NotifyReadComplete(result);
  }

  ServiceWorkerWriteToCacheJob* owner_;
  base::WeakPtrFactory<PassThroughConsumer> weak_factory_;
};

// Compares an existing resource with data progressively fed to it.
// Calls back to |owner|->OnCompareComplete once done.
class ServiceWorkerWriteToCacheJob::Comparer
    : public ServiceWorkerWriteToCacheJob::NetDataConsumer {
 public:
  Comparer(ServiceWorkerWriteToCacheJob* owner,
           scoped_ptr<ServiceWorkerResponseReader> reader)
      : owner_(owner), reader_(reader.release()), weak_factory_(this) {}
  ~Comparer() override {}

  void OnResponseStarted() override {
    owner_->CommitHeadersAndNotifyHeadersComplete();
  }

  void HandleData(net::IOBuffer* buf, int size) override {
    net_data_ = buf;
    net_data_offset_ = 0;
    net_data_size_ = size;

    if (size == 0 && info_) {
      Complete(bytes_matched_ == info_->response_data_size);
      return;
    }

    if (!info_) {
      read_buffer_ = new net::IOBuffer(kBufferSize);
      info_ = new HttpResponseInfoIOBuffer;
      reader_->ReadInfo(info_.get(), base::Bind(&Comparer::OnReadInfoComplete,
                                                weak_factory_.GetWeakPtr()));
      owner_->SetPendingIO();
      return;
    }

    ReadSomeData();
    owner_->SetPendingIO();
  }

 private:
  int bytes_remaining() {
    DCHECK(net_data_);
    DCHECK_LE(0, net_data_offset_);
    DCHECK_LE(net_data_offset_, net_data_size_);
    return net_data_size_ - net_data_offset_;
  }

  void OnReadInfoComplete(int result) {
    if (result < 0) {
      Complete(false);
      return;
    }

    if (bytes_remaining() == 0) {
      Complete(bytes_matched_ == info_->response_data_size);
      return;
    }

    ReadSomeData();
  }

  void ReadSomeData() {
    DCHECK_LT(0, bytes_remaining());
    int bytes_to_read = std::min(bytes_remaining(), kBufferSize);
    reader_->ReadData(
        read_buffer_.get(), bytes_to_read,
        base::Bind(&Comparer::OnReadDataComplete, weak_factory_.GetWeakPtr()));
  }

  void OnReadDataComplete(int result) {
    if (result <= 0) {
      // We hit error or EOF but had more to compare.
      Complete(false);
      return;
    }

    DCHECK_LE(result, bytes_remaining());
    if (memcmp(net_data_->data() + net_data_offset_, read_buffer_->data(),
               result) != 0) {
      Complete(false);
      return;
    }

    net_data_offset_ += result;
    if (bytes_remaining() == 0) {
      NotifyReadComplete();
      return;
    }

    ReadSomeData();
  }

  // Completes one HandleData() call.
  void NotifyReadComplete() {
    int size = net_data_size_;
    net_data_ = nullptr;
    net_data_offset_ = 0;
    net_data_size_ = 0;

    bytes_matched_ += size;
    owner_->ClearPendingIO();
    owner_->NotifyReadComplete(size);
  }

  // Completes the entire Comparer.
  void Complete(bool is_equal) {
    owner_->OnCompareComplete(bytes_matched_, is_equal);
  }

  ServiceWorkerWriteToCacheJob* owner_;
  scoped_ptr<ServiceWorkerResponseReader> reader_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<HttpResponseInfoIOBuffer> info_;

  // Cumulative number of bytes successfully compared.
  int bytes_matched_ = 0;

  // State used for one HandleData() call.
  scoped_refptr<net::IOBuffer> net_data_;
  int net_data_offset_ = 0;
  int net_data_size_ = 0;

  base::WeakPtrFactory<Comparer> weak_factory_;
};

ServiceWorkerWriteToCacheJob::ServiceWorkerWriteToCacheJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    ResourceType resource_type,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* version,
    int extra_load_flags,
    int64 response_id,
    int64 incumbent_response_id)
    : net::URLRequestJob(request, network_delegate),
      resource_type_(resource_type),
      context_(context),
      url_(request->url()),
      response_id_(response_id),
      incumbent_response_id_(incumbent_response_id),
      version_(version),
      has_been_killed_(false),
      did_notify_started_(false),
      did_notify_finished_(false),
      weak_factory_(this) {
  InitNetRequest(extra_load_flags);
}

ServiceWorkerWriteToCacheJob::~ServiceWorkerWriteToCacheJob() {
  DCHECK_EQ(did_notify_started_, did_notify_finished_);
}

void ServiceWorkerWriteToCacheJob::Start() {
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerWriteToCacheJob::ExecutingJob",
                           this,
                           "URL", request_->url().spec());
  if (!context_) {
    NotifyStartError(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }
  if (incumbent_response_id_ != kInvalidServiceWorkerResourceId) {
    scoped_ptr<ServiceWorkerResponseReader> incumbent_reader =
        context_->storage()->CreateResponseReader(incumbent_response_id_);
    consumer_.reset(new Comparer(this, incumbent_reader.Pass()));
  } else {
    consumer_.reset(new PassThroughConsumer(this));
  }

  version_->script_cache_map()->NotifyStartedCaching(
      url_, response_id_);
  did_notify_started_ = true;
  StartNetRequest();
}

void ServiceWorkerWriteToCacheJob::Kill() {
  if (has_been_killed_)
    return;
  weak_factory_.InvalidateWeakPtrs();
  has_been_killed_ = true;
  net_request_.reset();
  if (did_notify_started_ && !did_notify_finished_) {
    version_->script_cache_map()->NotifyFinishedCaching(
        url_, -1,
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_ABORTED),
        kKilledError);
    did_notify_finished_ = true;
  }
  writer_.reset();
  context_.reset();
  net::URLRequestJob::Kill();
}

net::LoadState ServiceWorkerWriteToCacheJob::GetLoadState() const {
  if (writer_ && writer_->IsWritePending())
    return net::LOAD_STATE_WAITING_FOR_APPCACHE;
  if (net_request_)
    return net_request_->GetLoadState().state;
  return net::LOAD_STATE_IDLE;
}

bool ServiceWorkerWriteToCacheJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

bool ServiceWorkerWriteToCacheJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

void ServiceWorkerWriteToCacheJob::GetResponseInfo(
    net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
}

int ServiceWorkerWriteToCacheJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

void ServiceWorkerWriteToCacheJob::SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) {
  std::string value;
  DCHECK(!headers.GetHeader(net::HttpRequestHeaders::kRange, &value));
  net_request_->SetExtraRequestHeaders(headers);
}

bool ServiceWorkerWriteToCacheJob::ReadRawData(net::IOBuffer* buf,
                                               int buf_size,
                                               int* bytes_read) {
  net::URLRequestStatus status = ReadNetData(buf, buf_size, bytes_read);
  SetStatus(status);
  if (status.is_io_pending())
    return false;

  if (!status.is_success()) {
    AsyncNotifyDoneHelper(status, kFetchScriptError);
    return false;
  }

  DCHECK_EQ(0, *bytes_read);
  consumer_->HandleData(buf, 0);
  if (did_notify_finished_)
    return GetStatus().is_success();
  if (GetStatus().is_io_pending())
    return false;
  return status.is_success();
}

const net::HttpResponseInfo* ServiceWorkerWriteToCacheJob::http_info() const {
  return http_info_.get();
}

void ServiceWorkerWriteToCacheJob::InitNetRequest(
    int extra_load_flags) {
  DCHECK(request());
  net_request_ = request()->context()->CreateRequest(
      request()->url(), request()->priority(), this);
  net_request_->set_first_party_for_cookies(
      request()->first_party_for_cookies());
  net_request_->SetReferrer(request()->referrer());
  if (extra_load_flags)
    net_request_->SetLoadFlags(net_request_->load_flags() | extra_load_flags);

  if (resource_type_ == RESOURCE_TYPE_SERVICE_WORKER) {
    // This will get copied into net_request_ when URLRequest::StartJob calls
    // ServiceWorkerWriteToCacheJob::SetExtraRequestHeaders.
    request()->SetExtraRequestHeaderByName("Service-Worker", "script", true);
  }
}

void ServiceWorkerWriteToCacheJob::StartNetRequest() {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerWriteToCacheJob::ExecutingJob",
                               this,
                               "NetRequest");
  net_request_->Start();  // We'll continue in OnResponseStarted.
}

net::URLRequestStatus ServiceWorkerWriteToCacheJob::ReadNetData(
    net::IOBuffer* buf,
    int buf_size,
    int* bytes_read) {
  DCHECK_GT(buf_size, 0);
  DCHECK(bytes_read);
  *bytes_read = 0;
  io_buffer_ = buf;
  io_buffer_bytes_ = 0;
  int net_bytes_read = 0;
  if (!net_request_->Read(buf, buf_size, &net_bytes_read)) {
    if (net_request_->status().is_io_pending())
      return net_request_->status();
    DCHECK(!net_request_->status().is_success());
    return net_request_->status();
  }

  if (net_bytes_read != 0) {
    HandleNetData(net_bytes_read);
    DCHECK(GetStatus().is_io_pending());
    return GetStatus();
  }

  DCHECK(net_request_->status().is_success());
  return net_request_->status();
}

void ServiceWorkerWriteToCacheJob::WriteHeaders(const base::Closure& callback) {
  if (!context_) {
    AsyncNotifyDoneHelper(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED),
        kFetchScriptError);
    return;
  }
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerWriteToCacheJob::ExecutingJob",
                               this, "WriteHeaders");
  writer_ = context_->storage()->CreateResponseWriter(response_id_);
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer(
          new net::HttpResponseInfo(net_request_->response_info()));
  writer_->WriteInfo(
      info_buffer.get(),
      base::Bind(&ServiceWorkerWriteToCacheJob::OnWriteHeadersComplete,
                 weak_factory_.GetWeakPtr(), callback));
}

void ServiceWorkerWriteToCacheJob::OnWriteHeadersComplete(
    const base::Closure& callback,
    int result) {
  if (result < 0) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_HEADERS_ERROR);
    AsyncNotifyDoneHelper(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, result),
        kFetchScriptError);
    return;
  }
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerWriteToCacheJob::ExecutingJob",
                               this, "WriteHeadersCompleted");
  callback.Run();
}

void ServiceWorkerWriteToCacheJob::WriteData(
    net::IOBuffer* buf,
    int bytes_to_write,
    const base::Callback<void(int result)>& callback) {
  DCHECK_LT(0, bytes_to_write);
  TRACE_EVENT_ASYNC_STEP_INTO1(
      "ServiceWorker", "ServiceWorkerWriteToCacheJob::ExecutingJob", this,
      "WriteData", "Amount to write", bytes_to_write);

  writer_->WriteData(
      buf, bytes_to_write,
      base::Bind(&ServiceWorkerWriteToCacheJob::OnWriteDataComplete,
                 weak_factory_.GetWeakPtr(), callback));
}

void ServiceWorkerWriteToCacheJob::OnWriteDataComplete(
    const base::Callback<void(int result)>& callback,
    int result) {
  DCHECK_NE(0, result);
  if (!context_) {
    AsyncNotifyDoneHelper(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED),
        kFetchScriptError);
    return;
  }
  if (result < 0) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_DATA_ERROR);
    AsyncNotifyDoneHelper(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, result),
        kFetchScriptError);
    return;
  }
  ServiceWorkerMetrics::CountWriteResponseResult(
      ServiceWorkerMetrics::WRITE_OK);
  callback.Run(result);
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerWriteToCacheJob::ExecutingJob", this);
}

void ServiceWorkerWriteToCacheJob::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  DCHECK_EQ(net_request_, request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnReceivedRedirect");
  // Script resources can't redirect.
  AsyncNotifyDoneHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                              net::ERR_UNSAFE_REDIRECT),
                        kRedirectError);
}

void ServiceWorkerWriteToCacheJob::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  DCHECK_EQ(net_request_, request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnAuthRequired");
  // TODO(michaeln): Pass this thru to our jobs client.
  AsyncNotifyDoneHelper(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED),
      kClientAuthenticationError);
}

void ServiceWorkerWriteToCacheJob::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK_EQ(net_request_, request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnCertificateRequested");
  // TODO(michaeln): Pass this thru to our jobs client.
  // see NotifyCertificateRequested.
  AsyncNotifyDoneHelper(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED),
      kClientAuthenticationError);
}

void ServiceWorkerWriteToCacheJob::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK_EQ(net_request_, request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnSSLCertificateError");
  // TODO(michaeln): Pass this thru to our jobs client,
  // see NotifySSLCertificateError.
  AsyncNotifyDoneHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                              net::ERR_INSECURE_RESPONSE),
                        kSSLError);
}

void ServiceWorkerWriteToCacheJob::OnBeforeNetworkStart(
    net::URLRequest* request,
    bool* defer) {
  DCHECK_EQ(net_request_, request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnBeforeNetworkStart");
  NotifyBeforeNetworkStart(defer);
}

void ServiceWorkerWriteToCacheJob::OnResponseStarted(
    net::URLRequest* request) {
  DCHECK_EQ(net_request_, request);
  if (!request->status().is_success()) {
    AsyncNotifyDoneHelper(request->status(), kFetchScriptError);
    return;
  }
  if (request->GetResponseCode() / 100 != 2) {
    std::string error_message =
        base::StringPrintf(kBadHTTPResponseError, request->GetResponseCode());
    AsyncNotifyDoneHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                                net::ERR_INVALID_RESPONSE),
                          error_message);
    // TODO(michaeln): Instead of error'ing immediately, send the net
    // response to our consumer, just don't cache it?
    return;
  }
  // OnSSLCertificateError is not called when the HTTPS connection is reused.
  // So we check cert_status here.
  if (net::IsCertStatusError(request->ssl_info().cert_status)) {
    const net::HttpNetworkSession::Params* session_params =
        request->context()->GetNetworkSessionParams();
    if (!session_params || !session_params->ignore_certificate_errors) {
      AsyncNotifyDoneHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                                  net::ERR_INSECURE_RESPONSE),
                            kSSLError);
      return;
    }
  }

  if (version_->script_url() == url_) {
    std::string mime_type;
    request->GetMimeType(&mime_type);
    if (mime_type != "application/x-javascript" &&
        mime_type != "text/javascript" &&
        mime_type != "application/javascript") {
      std::string error_message =
          mime_type.empty()
              ? kNoMIMEError
              : base::StringPrintf(kBadMIMEError, mime_type.c_str());
      AsyncNotifyDoneHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                                  net::ERR_INSECURE_RESPONSE),
                            error_message);
      return;
    }

    if (!CheckPathRestriction(request))
      return;

    version_->SetMainScriptHttpResponseInfo(net_request_->response_info());
  }

  if (net_request_->response_info().network_accessed)
    version_->embedded_worker()->OnNetworkAccessedForScriptLoad();

  consumer_->OnResponseStarted();
}

void ServiceWorkerWriteToCacheJob::CommitHeadersAndNotifyHeadersComplete() {
  http_info_.reset(new net::HttpResponseInfo(net_request_->response_info()));
  NotifyHeadersComplete();
}

void ServiceWorkerWriteToCacheJob::HandleNetData(int bytes_read) {
  io_buffer_bytes_ = bytes_read;
  consumer_->HandleData(io_buffer_.get(), bytes_read);
}

void ServiceWorkerWriteToCacheJob::OnReadCompleted(
    net::URLRequest* request,
    int bytes_read) {
  DCHECK_EQ(net_request_, request);
  if (bytes_read < 0) {
    DCHECK(!request->status().is_success());
    AsyncNotifyDoneHelper(request->status(), kFetchScriptError);
    return;
  }
  if (bytes_read > 0) {
    HandleNetData(bytes_read);
    DCHECK(GetStatus().is_io_pending());
    return;
  }

  // No more data to process, the job is complete.
  DCHECK(request->status().is_success());
  HandleNetData(0);
}

bool ServiceWorkerWriteToCacheJob::CheckPathRestriction(
    net::URLRequest* request) {
  std::string service_worker_allowed;
  const net::HttpResponseHeaders* headers = request->response_headers();
  bool has_header = headers->EnumerateHeader(nullptr, kServiceWorkerAllowed,
                                             &service_worker_allowed);

  std::string error_message;
  if (!ServiceWorkerUtils::IsPathRestrictionSatisfied(
          version_->scope(), url_,
          has_header ? &service_worker_allowed : nullptr, &error_message)) {
    AsyncNotifyDoneHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                                net::ERR_INSECURE_RESPONSE),
                          error_message);
    return false;
  }
  return true;
}

void ServiceWorkerWriteToCacheJob::SetPendingIO() {
  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
}

void ServiceWorkerWriteToCacheJob::ClearPendingIO() {
  SetStatus(net::URLRequestStatus());
}

void ServiceWorkerWriteToCacheJob::OnPassThroughComplete() {
  NotifyFinishedCaching(net::URLRequestStatus(), std::string());
  if (GetStatus().is_io_pending()) {
    ClearPendingIO();
    NotifyReadComplete(0);
  }
}

void ServiceWorkerWriteToCacheJob::OnCompareComplete(int bytes_matched,
                                                     bool is_equal) {
  if (is_equal) {
    // This version is identical to the incumbent, so discard it and fail this
    // job.
    version_->SetStartWorkerStatusCode(SERVICE_WORKER_ERROR_EXISTS);
    AsyncNotifyDoneHelper(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED),
        kFetchScriptError);
    return;
  }

  // We must switch to the pass through consumer. Write what is known
  // (headers + bytes matched) to disk.
  WriteHeaders(base::Bind(&ServiceWorkerWriteToCacheJob::CopyIncumbent,
                          weak_factory_.GetWeakPtr(), bytes_matched));
  SetPendingIO();
}

void ServiceWorkerWriteToCacheJob::CopyIncumbent(int bytes_to_copy) {
  if (bytes_to_copy == 0) {
    OnCopyComplete(SERVICE_WORKER_OK);
    return;
  }
  scoped_ptr<ServiceWorkerResponseReader> incumbent_reader =
      context_->storage()->CreateResponseReader(incumbent_response_id_);
  scoped_refptr<Copier> copier = new Copier(
      weak_factory_.GetWeakPtr(), incumbent_reader.Pass(), bytes_to_copy,
      base::Bind(&ServiceWorkerWriteToCacheJob::OnCopyComplete,
                 weak_factory_.GetWeakPtr()));
  copier->Start();  // It deletes itself when done.
  DCHECK(GetStatus().is_io_pending());
}

void ServiceWorkerWriteToCacheJob::OnCopyComplete(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    AsyncNotifyDoneHelper(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED),
        kFetchScriptError);
    return;
  }

  // Continue processing the net data that triggered the comparison fail and
  // copy.
  if (io_buffer_bytes_ > 0) {
    consumer_.reset(new PassThroughConsumer(this));
    consumer_->HandleData(io_buffer_.get(), io_buffer_bytes_);
    return;
  }

  // The copy was triggered by EOF from the network, which
  // means the job is now done.
  NotifyFinishedCaching(net::URLRequestStatus(), std::string());
  ClearPendingIO();
  NotifyReadComplete(0);
}

void ServiceWorkerWriteToCacheJob::AsyncNotifyDoneHelper(
    const net::URLRequestStatus& status,
    const std::string& status_message) {
  DCHECK(!status.is_io_pending());
  NotifyFinishedCaching(status, status_message);
  SetStatus(status);
  NotifyDone(status);
}

void ServiceWorkerWriteToCacheJob::NotifyFinishedCaching(
    net::URLRequestStatus status,
    const std::string& status_message) {
  DCHECK(!did_notify_finished_);
  int size = -1;
  if (status.is_success())
    size = writer_ ? writer_->amount_written() : 0;
  version_->script_cache_map()->NotifyFinishedCaching(url_, size, status,
                                                      status_message);
  did_notify_finished_ = true;
}

}  // namespace content
