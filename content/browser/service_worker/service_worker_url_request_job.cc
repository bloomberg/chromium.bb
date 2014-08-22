// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_request_job.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/stringprintf.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/page_transition_types.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/browser/blob/blob_url_request_job_factory.h"

namespace content {

ServiceWorkerURLRequestJob::ServiceWorkerURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    scoped_refptr<ResourceRequestBody> body)
    : net::URLRequestJob(request, network_delegate),
      provider_host_(provider_host),
      response_type_(NOT_DETERMINED),
      is_started_(false),
      blob_storage_context_(blob_storage_context),
      body_(body),
      weak_factory_(this) {
}

void ServiceWorkerURLRequestJob::FallbackToNetwork() {
  DCHECK_EQ(NOT_DETERMINED, response_type_);
  response_type_ = FALLBACK_TO_NETWORK;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::ForwardToServiceWorker() {
  DCHECK_EQ(NOT_DETERMINED, response_type_);
  response_type_ = FORWARD_TO_SERVICE_WORKER;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Start() {
  is_started_ = true;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Kill() {
  net::URLRequestJob::Kill();
  fetch_dispatcher_.reset();
  blob_request_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

net::LoadState ServiceWorkerURLRequestJob::GetLoadState() const {
  // TODO(kinuko): refine this for better debug.
  return net::URLRequestJob::GetLoadState();
}

bool ServiceWorkerURLRequestJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

bool ServiceWorkerURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

void ServiceWorkerURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
}

int ServiceWorkerURLRequestJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

void ServiceWorkerURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  std::vector<net::HttpByteRange> ranges;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header) ||
      !net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
    return;
  }

  // We don't support multiple range requests in one single URL request.
  if (ranges.size() == 1U)
    byte_range_ = ranges[0];
}

bool ServiceWorkerURLRequestJob::ReadRawData(
    net::IOBuffer* buf, int buf_size, int *bytes_read) {
  if (!blob_request_) {
    *bytes_read = 0;
    return true;
  }

  blob_request_->Read(buf, buf_size, bytes_read);
  net::URLRequestStatus status = blob_request_->status();
  SetStatus(status);
  if (status.is_io_pending())
    return false;
  return status.is_success();
}

void ServiceWorkerURLRequestJob::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnBeforeNetworkStart(net::URLRequest* request,
                                                      bool* defer) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnResponseStarted(net::URLRequest* request) {
  // TODO(falken): Add Content-Length, Content-Type if they were not provided in
  // the ServiceWorkerResponse.
  CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::OnReadCompleted(net::URLRequest* request,
                                                 int bytes_read) {
  SetStatus(request->status());
  if (!request->status().is_success()) {
    NotifyDone(request->status());
    return;
  }
  NotifyReadComplete(bytes_read);
  if (bytes_read == 0)
    NotifyDone(request->status());
}

const net::HttpResponseInfo* ServiceWorkerURLRequestJob::http_info() const {
  if (!http_response_info_)
    return NULL;
  if (range_response_info_)
    return range_response_info_.get();
  return http_response_info_.get();
}

void ServiceWorkerURLRequestJob::GetExtraResponseInfo(
    bool* was_fetched_via_service_worker,
    GURL* original_url_via_service_worker) const {
  if (response_type_ != FORWARD_TO_SERVICE_WORKER) {
    *was_fetched_via_service_worker = false;
    *original_url_via_service_worker = GURL();
    return;
  }
  *was_fetched_via_service_worker = true;
  *original_url_via_service_worker = response_url_;
}


ServiceWorkerURLRequestJob::~ServiceWorkerURLRequestJob() {
}

void ServiceWorkerURLRequestJob::MaybeStartRequest() {
  if (is_started_ && response_type_ != NOT_DETERMINED) {
    // Start asynchronously.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ServiceWorkerURLRequestJob::StartRequest,
                   weak_factory_.GetWeakPtr()));
  }
}

void ServiceWorkerURLRequestJob::StartRequest() {
  switch (response_type_) {
    case NOT_DETERMINED:
      NOTREACHED();
      return;

    case FALLBACK_TO_NETWORK:
      // Restart the request to create a new job. Our request handler will
      // return NULL, and the default job (which will hit network) should be
      // created.
      NotifyRestartRequired();
      return;

    case FORWARD_TO_SERVICE_WORKER:
      DCHECK(provider_host_ && provider_host_->active_version());
      DCHECK(!fetch_dispatcher_);
      // Send a fetch event to the ServiceWorker associated to the
      // provider_host.
      fetch_dispatcher_.reset(new ServiceWorkerFetchDispatcher(
          CreateFetchRequest(),
          provider_host_->active_version(),
          base::Bind(&ServiceWorkerURLRequestJob::DidDispatchFetchEvent,
                     weak_factory_.GetWeakPtr())));
      fetch_dispatcher_->Run();
      return;
  }

  NOTREACHED();
}

scoped_ptr<ServiceWorkerFetchRequest>
ServiceWorkerURLRequestJob::CreateFetchRequest() {
  std::string blob_uuid;
  uint64 blob_size = 0;
  CreateRequestBodyBlob(&blob_uuid, &blob_size);
  scoped_ptr<ServiceWorkerFetchRequest> request(
      new ServiceWorkerFetchRequest());

  request->url = request_->url();
  request->method = request_->method();
  const net::HttpRequestHeaders& headers = request_->extra_request_headers();
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();)
    request->headers[it.name()] = it.value();
  request->blob_uuid = blob_uuid;
  request->blob_size = blob_size;
  request->referrer = GURL(request_->referrer());
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (info) {
    request->is_reload = PageTransitionCoreTypeIs(info->GetPageTransition(),
                                                  PAGE_TRANSITION_RELOAD);
  }
  return request.Pass();
}

bool ServiceWorkerURLRequestJob::CreateRequestBodyBlob(std::string* blob_uuid,
                                                       uint64* blob_size) {
  if (!body_ || !blob_storage_context_)
    return false;
  const std::string uuid(base::GenerateGUID());
  uint64 size = 0;
  std::vector<const ResourceRequestBody::Element*> resolved_elements;
  for (size_t i = 0; i < body_->elements()->size(); ++i) {
    const ResourceRequestBody::Element& element = (*body_->elements())[i];
    if (element.type() != ResourceRequestBody::Element::TYPE_BLOB) {
      resolved_elements.push_back(&element);
      continue;
    }
    scoped_ptr<storage::BlobDataHandle> handle =
        blob_storage_context_->GetBlobDataFromUUID(element.blob_uuid());
    if (handle->data()->items().empty())
      continue;
    for (size_t i = 0; i < handle->data()->items().size(); ++i) {
      const storage::BlobData::Item& item = handle->data()->items().at(i);
      DCHECK_NE(storage::BlobData::Item::TYPE_BLOB, item.type());
      resolved_elements.push_back(&item);
    }
  }
  scoped_refptr<storage::BlobData> blob_data = new storage::BlobData(uuid);
  for (size_t i = 0; i < resolved_elements.size(); ++i) {
    const ResourceRequestBody::Element& element = *resolved_elements[i];
    size += element.length();
    switch (element.type()) {
      case ResourceRequestBody::Element::TYPE_BYTES:
        blob_data->AppendData(element.bytes(), element.length());
        break;
      case ResourceRequestBody::Element::TYPE_FILE:
        blob_data->AppendFile(element.path(),
                              element.offset(),
                              element.length(),
                              element.expected_modification_time());
        break;
      case ResourceRequestBody::Element::TYPE_BLOB:
        // Blob elements should be resolved beforehand.
        NOTREACHED();
        break;
      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        blob_data->AppendFileSystemFile(element.filesystem_url(),
                                        element.offset(),
                                        element.length(),
                                        element.expected_modification_time());
        break;
      default:
        NOTIMPLEMENTED();
    }
  }

  request_body_blob_data_handle_ =
      blob_storage_context_->AddFinishedBlob(blob_data.get());
  *blob_uuid = uuid;
  *blob_size = size;
  return true;
}

void ServiceWorkerURLRequestJob::DidDispatchFetchEvent(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  fetch_dispatcher_.reset();

  // Check if we're not orphaned.
  if (!request())
    return;

  if (status != SERVICE_WORKER_OK) {
    // Dispatching event has been failed, falling back to the network.
    // (Tentative behavior described on github)
    // TODO(kinuko): consider returning error if we've come here because
    // unexpected worker termination etc (so that we could fix bugs).
    // TODO(kinuko): Would be nice to log the error case.
    response_type_ = FALLBACK_TO_NETWORK;
    NotifyRestartRequired();
    return;
  }

  if (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK) {
    // Change the response type and restart the request to fallback to
    // the network.
    response_type_ = FALLBACK_TO_NETWORK;
    NotifyRestartRequired();
    return;
  }

  // We should have a response now.
  DCHECK_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, fetch_result);

  // Set up a request for reading the blob.
  if (!response.blob_uuid.empty() && blob_storage_context_) {
    scoped_ptr<storage::BlobDataHandle> blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response.blob_uuid);
    if (!blob_data_handle) {
      // The renderer gave us a bad blob UUID.
      DeliverErrorResponse();
      return;
    }
    blob_request_ = storage::BlobProtocolHandler::CreateBlobRequest(
        blob_data_handle.Pass(), request()->context(), this);
    blob_request_->Start();
  }

  response_url_ = response.url;
  CreateResponseHeader(
      response.status_code, response.status_text, response.headers);
  if (!blob_request_)
    CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::CreateResponseHeader(
    int status_code,
    const std::string& status_text,
    const std::map<std::string, std::string>& headers) {
  // TODO(kinuko): If the response has an identifier to on-disk cache entry,
  // pull response header from the disk.
  std::string status_line(
      base::StringPrintf("HTTP/1.1 %d %s", status_code, status_text.c_str()));
  status_line.push_back('\0');
  http_response_headers_ = new net::HttpResponseHeaders(status_line);
  for (std::map<std::string, std::string>::const_iterator it = headers.begin();
       it != headers.end();
       ++it) {
    std::string header;
    header.reserve(it->first.size() + 2 + it->second.size());
    header.append(it->first);
    header.append(": ");
    header.append(it->second);
    http_response_headers_->AddHeader(header);
  }
}

void ServiceWorkerURLRequestJob::CommitResponseHeader() {
  http_response_info_.reset(new net::HttpResponseInfo());
  http_response_info_->headers.swap(http_response_headers_);
  NotifyHeadersComplete();
}

void ServiceWorkerURLRequestJob::DeliverErrorResponse() {
  // TODO(falken): Print an error to the console of the ServiceWorker and of
  // the requesting page.
  CreateResponseHeader(500,
                       "Service Worker Response Error",
                       std::map<std::string, std::string>());
  CommitResponseHeader();
}

}  // namespace content
