// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_REQUEST_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_REQUEST_JOB_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/streams/stream_read_observer.h"
#include "content/browser/streams/stream_register_observer.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerResponseType.h"
#include "url/gurl.h"

namespace net {
class IOBuffer;
}

namespace storage {
class BlobDataHandle;
class BlobStorageContext;
}

namespace content {

class ResourceContext;
class ResourceRequestBody;
class ServiceWorkerContextCore;
class ServiceWorkerFetchDispatcher;
class ServiceWorkerProviderHost;
class ServiceWorkerVersion;
class Stream;

class CONTENT_EXPORT ServiceWorkerURLRequestJob
    : public net::URLRequestJob,
      public net::URLRequest::Delegate,
      public StreamReadObserver,
      public StreamRegisterObserver {
 public:
  ServiceWorkerURLRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      const ResourceContext* resource_context,
      FetchRequestMode request_mode,
      FetchCredentialsMode credentials_mode,
      RequestContextType request_context_type,
      RequestContextFrameType frame_type,
      scoped_refptr<ResourceRequestBody> body);

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
  void Start() override;
  void Kill() override;
  net::LoadState GetLoadState() const override;
  bool GetCharset(std::string* charset) override;
  bool GetMimeType(std::string* mime_type) const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  void GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;
  int GetResponseCode() const override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read) override;

  // net::URLRequest::Delegate overrides that read the blob from the
  // ServiceWorkerFetchResponse.
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* auth_info) override;
  void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;
  void OnBeforeNetworkStart(net::URLRequest* request, bool* defer) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  // StreamObserver override:
  void OnDataAvailable(Stream* stream) override;

  // StreamRegisterObserver override:
  void OnStreamRegistered(Stream* stream) override;

  void GetExtraResponseInfo(
      bool* was_fetched_via_service_worker,
      bool* was_fallback_required_by_service_worker,
      GURL* original_url_via_service_worker,
      blink::WebServiceWorkerResponseType* response_type_via_service_worker,
      base::TimeTicks* fetch_start_time,
      base::TimeTicks* fetch_ready_time,
      base::TimeTicks* fetch_end_time) const;

 protected:
  ~ServiceWorkerURLRequestJob() override;

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

  // Creates ServiceWorkerFetchRequest from |request_| and |body_|.
  scoped_ptr<ServiceWorkerFetchRequest> CreateFetchRequest();

  // Creates BlobDataHandle of the request body from |body_|. This handle
  // |request_body_blob_data_handle_| will be deleted when
  // ServiceWorkerURLRequestJob is deleted.
  bool CreateRequestBodyBlob(std::string* blob_uuid, uint64* blob_size);

  // For FORWARD_TO_SERVICE_WORKER case.
  void DidPrepareFetchEvent();
  void DidDispatchFetchEvent(ServiceWorkerStatusCode status,
                             ServiceWorkerFetchEventResult fetch_result,
                             const ServiceWorkerResponse& response);

  // Populates |http_response_headers_|.
  void CreateResponseHeader(int status_code,
                            const std::string& status_text,
                            const ServiceWorkerHeaderMap& headers);

  // Creates |http_response_info_| using |http_response_headers_| and calls
  // NotifyHeadersComplete.
  void CommitResponseHeader();

  // Creates and commits a response header indicating error.
  void DeliverErrorResponse();

  // Releases the resources for streaming.
  void ClearStream();

  const net::HttpResponseInfo* http_info() const;

  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;

  // Timing info to show on the popup in Devtools' Network tab.
  net::LoadTimingInfo load_timing_info_;
  base::TimeTicks fetch_start_time_;
  base::TimeTicks fetch_ready_time_;
  base::TimeTicks fetch_end_time_;
  base::Time response_time_;

  ResponseType response_type_;
  bool is_started_;

  net::HttpByteRange byte_range_;
  scoped_ptr<net::HttpResponseInfo> range_response_info_;
  scoped_ptr<net::HttpResponseInfo> http_response_info_;
  // Headers that have not yet been committed to |http_response_info_|.
  scoped_refptr<net::HttpResponseHeaders> http_response_headers_;
  GURL response_url_;
  blink::WebServiceWorkerResponseType service_worker_response_type_;

  // Used when response type is FORWARD_TO_SERVICE_WORKER.
  scoped_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  const ResourceContext* resource_context_;
  scoped_ptr<net::URLRequest> blob_request_;
  scoped_refptr<Stream> stream_;
  GURL waiting_stream_url_;
  scoped_refptr<net::IOBuffer> stream_pending_buffer_;
  int stream_pending_buffer_size_;

  FetchRequestMode request_mode_;
  FetchCredentialsMode credentials_mode_;
  RequestContextType request_context_type_;
  RequestContextFrameType frame_type_;
  bool fall_back_required_;
  // ResourceRequestBody has a collection of BlobDataHandles attached to it
  // using the userdata mechanism. So we have to keep it not to free the blobs.
  scoped_refptr<ResourceRequestBody> body_;
  scoped_ptr<storage::BlobDataHandle> request_body_blob_data_handle_;
  scoped_refptr<ServiceWorkerVersion> streaming_version_;

  base::WeakPtrFactory<ServiceWorkerURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_REQUEST_JOB_H_
