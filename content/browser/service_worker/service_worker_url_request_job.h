// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_REQUEST_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_REQUEST_JOB_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerFetchDispatcher;
class ServiceWorkerProviderHost;

class CONTENT_EXPORT ServiceWorkerURLRequestJob
    : public net::URLRequestJob {
 public:
  ServiceWorkerURLRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host);

  // Sets the response type.
  void FallbackToNetwork();
  void ForwardToServiceWorker();

  bool ShouldFallbackToNetwork() const {
    return response_type_ == FALLBACK_TO_NETWORK;
  }
  bool ShouldForwardToServiceWorker() const {
    return response_type_ == FORWARD_TO_SERVICE_WORKER;
  }

  // net::URLRequestJob overrides:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual net::LoadState GetLoadState() const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int *bytes_read) OVERRIDE;

  const net::HttpResponseInfo* http_info() const;

 protected:
  virtual ~ServiceWorkerURLRequestJob();

 private:
  enum ResponseType {
    NOT_DETERMINED,
    FALLBACK_TO_NETWORK,
    FORWARD_TO_SERVICE_WORKER,
  };

  // We start processing the request if Start() is called AND response_type_
  // is determined.
  void MaybeStartRequest();
  void StartRequest();

  // For FORWARD_TO_SERVICE_WORKER case.
  void DidDispatchFetchEvent(ServiceWorkerStatusCode status,
                             ServiceWorkerFetchEventResult fetch_result,
                             const ServiceWorkerResponse& response);

  void CreateResponseHeader(const ServiceWorkerResponse& response);

  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;

  ResponseType response_type_;
  bool is_started_;

  net::HttpByteRange byte_range_;
  scoped_ptr<net::HttpResponseInfo> range_response_info_;
  scoped_ptr<net::HttpResponseInfo> http_response_info_;

  // Used when response type is FORWARD_TO_SERVICE_WORKER.
  scoped_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;

  base::WeakPtrFactory<ServiceWorkerURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_REQUEST_JOB_H_
