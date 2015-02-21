// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_write_to_cache_job.h"

#include "base/profiler/scoped_tracker.h"
#include "base/strings/stringprintf.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/service_worker/service_worker_types.h"
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

}

ServiceWorkerWriteToCacheJob::ServiceWorkerWriteToCacheJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    ResourceType resource_type,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* version,
    int extra_load_flags,
    int64 response_id)
    : net::URLRequestJob(request, network_delegate),
      resource_type_(resource_type),
      context_(context),
      url_(request->url()),
      response_id_(response_id),
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

bool ServiceWorkerWriteToCacheJob::ReadRawData(
    net::IOBuffer* buf,
    int buf_size,
    int *bytes_read) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 ServiceWorkerWriteToCacheJob::ReadRawData"));

  net::URLRequestStatus status = ReadNetData(buf, buf_size, bytes_read);
  SetStatus(status);
  if (status.is_io_pending())
    return false;

  // No more data to process, the job is complete.
  io_buffer_ = NULL;
  version_->script_cache_map()->NotifyFinishedCaching(
      url_, writer_->amount_written(), status, std::string());
  did_notify_finished_ = true;
  return status.is_success();
}

const net::HttpResponseInfo* ServiceWorkerWriteToCacheJob::http_info() const {
  return http_info_.get();
}

void ServiceWorkerWriteToCacheJob::InitNetRequest(
    int extra_load_flags) {
  DCHECK(request());
  net_request_ = request()->context()->CreateRequest(
      request()->url(),
      request()->priority(),
      this,
      this->GetCookieStore());
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
    int *bytes_read) {
  DCHECK_GT(buf_size, 0);
  DCHECK(bytes_read);

  *bytes_read = 0;
  io_buffer_ = buf;
  int net_bytes_read = 0;
  if (!net_request_->Read(buf, buf_size, &net_bytes_read)) {
    if (net_request_->status().is_io_pending())
      return net_request_->status();
    DCHECK(!net_request_->status().is_success());
    return net_request_->status();
  }

  if (net_bytes_read != 0) {
    WriteDataToCache(net_bytes_read);
    DCHECK(GetStatus().is_io_pending());
    return GetStatus();
  }

  DCHECK(net_request_->status().is_success());
  return net_request_->status();
}

void ServiceWorkerWriteToCacheJob::WriteHeadersToCache() {
  if (!context_) {
    AsyncNotifyDoneHelper(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED),
        kFetchScriptError);
    return;
  }
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerWriteToCacheJob::ExecutingJob",
                               this,
                               "WriteHeadersToCache");
  writer_ = context_->storage()->CreateResponseWriter(response_id_);
  info_buffer_ = new HttpResponseInfoIOBuffer(
      new net::HttpResponseInfo(net_request_->response_info()));
  writer_->WriteInfo(
      info_buffer_.get(),
      base::Bind(&ServiceWorkerWriteToCacheJob::OnWriteHeadersComplete,
                 weak_factory_.GetWeakPtr()));
  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
}

void ServiceWorkerWriteToCacheJob::OnWriteHeadersComplete(int result) {
  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status
  if (result < 0) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_HEADERS_ERROR);
    AsyncNotifyDoneHelper(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, result),
        kFetchScriptError);
    return;
  }
  http_info_.reset(info_buffer_->http_info.release());
  info_buffer_ = NULL;
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerWriteToCacheJob::ExecutingJob",
                               this,
                               "WriteHeadersToCacheCompleted");
  NotifyHeadersComplete();
}

void ServiceWorkerWriteToCacheJob::WriteDataToCache(int amount_to_write) {
  DCHECK_NE(0, amount_to_write);
  TRACE_EVENT_ASYNC_STEP_INTO1("ServiceWorker",
                               "ServiceWorkerWriteToCacheJob::ExecutingJob",
                               this,
                               "WriteDataToCache",
                               "Amount to write", amount_to_write);
  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  writer_->WriteData(
      io_buffer_.get(),
      amount_to_write,
      base::Bind(&ServiceWorkerWriteToCacheJob::OnWriteDataComplete,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerWriteToCacheJob::OnWriteDataComplete(int result) {
  DCHECK_NE(0, result);
  io_buffer_ = NULL;
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
  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status
  NotifyReadComplete(result);
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerWriteToCacheJob::ExecutingJob",
                         this);
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
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 ServiceWorkerWriteToCacheJob::OnResponseStarted"));

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

  WriteHeadersToCache();
}

void ServiceWorkerWriteToCacheJob::OnReadCompleted(
    net::URLRequest* request,
    int bytes_read) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 ServiceWorkerWriteToCacheJob::OnReadCompleted"));

  DCHECK_EQ(net_request_, request);
  if (bytes_read < 0) {
    DCHECK(!request->status().is_success());
    AsyncNotifyDoneHelper(request->status(), kFetchScriptError);
    return;
  }
  if (bytes_read > 0) {
    WriteDataToCache(bytes_read);
    return;
  }
  // No more data to process, the job is complete.
  DCHECK(request->status().is_success());
  io_buffer_ = NULL;
  version_->script_cache_map()->NotifyFinishedCaching(
      url_, writer_->amount_written(), net::URLRequestStatus(), std::string());
  did_notify_finished_ = true;
  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status
  NotifyReadComplete(0);
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

void ServiceWorkerWriteToCacheJob::AsyncNotifyDoneHelper(
    const net::URLRequestStatus& status,
    const std::string& status_message) {
  DCHECK(!status.is_io_pending());
  DCHECK(!did_notify_finished_);
  int size = -1;
  if (writer_.get()) {
    size = writer_->amount_written();
  }
  version_->script_cache_map()->NotifyFinishedCaching(url_, size, status,
                                                      status_message);
  did_notify_finished_ = true;
  SetStatus(status);
  NotifyDone(status);
}

}  // namespace content
