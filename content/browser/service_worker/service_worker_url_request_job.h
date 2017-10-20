// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_REQUEST_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_REQUEST_JOB_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_response_type.h"
#include "content/browser/service_worker/service_worker_url_job_wrapper.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/service_worker_modes.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "url/gurl.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace storage {
class BlobDataHandle;
class BlobStorageContext;
}  // namespace storage

namespace content {

class ResourceContext;
class ResourceRequestBody;
class ServiceWorkerBlobReader;
class ServiceWorkerDataPipeReader;
class ServiceWorkerFetchDispatcher;
class ServiceWorkerVersion;

class CONTENT_EXPORT ServiceWorkerURLRequestJob : public net::URLRequestJob {
 public:
  using Delegate = ServiceWorkerURLJobWrapper::Delegate;

  ServiceWorkerURLRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const std::string& client_id,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      const ResourceContext* resource_context,
      FetchRequestMode request_mode,
      FetchCredentialsMode credentials_mode,
      FetchRedirectMode redirect_mode,
      const std::string& integrity,
      ResourceType resource_type,
      RequestContextType request_context_type,
      RequestContextFrameType frame_type,
      scoped_refptr<ResourceRequestBody> body,
      ServiceWorkerFetchType fetch_type,
      const base::Optional<base::TimeDelta>& timeout,
      Delegate* delegate);

  ~ServiceWorkerURLRequestJob() override;

  const ResourceContext* resource_context() const { return resource_context_; }
  bool did_navigation_preload() const { return did_navigation_preload_; }

  // When set, this job will pretend that navigation preload was triggered, but
  // not actually do navigation preload. This is to aid unit tests that check
  // UMA logging. It's difficult to get navigation preload to work in a unit
  // test: there is a much setup of ResourceRequestInfoImpl,
  // ResourceRequesterInfo, etc. required.
  void set_simulate_navigation_preload_for_test() {
    simulate_navigation_preload_for_test_ = true;
  }

  // Sets the response type.
  // When an in-flight request possibly needs CORS check, use
  // FallbackToNetworkOrRenderer. This method will decide whether the request
  // can directly go to the network or should fallback to a renderer to send
  // CORS preflight. You can use FallbackToNetwork only when, like main resource
  // or foreign fetch cases, it's apparent that the request should go to the
  // network directly.
  // TODO(shimazu): Update the comment when what should we do at foreign fetch
  // fallback is determined: crbug.com/604084
  void FallbackToNetwork();
  void FallbackToNetworkOrRenderer();
  void ForwardToServiceWorker();
  // Tells the job to abort with a start error. Currently this is only called
  // because the controller was lost. This function could be made more generic
  // if needed later.
  void FailDueToLostController();

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
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;

  //----------------------------------------------------------------------------
  // The following are intended for use by ServiceWorker(Blob|DataPipe)Reader.
  virtual void OnResponseStarted();
  virtual void OnReadRawDataComplete(int bytes_read);
  virtual void RecordResult(ServiceWorkerMetrics::URLRequestJobResult result);
  //----------------------------------------------------------------------------

  base::WeakPtr<ServiceWorkerURLRequestJob> GetWeakPtr();

 private:
  class FileSizeResolver;
  class NavigationPreloadMetrics;
  friend class DelayHelper;
  friend class ServiceWorkerURLRequestJobTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           LostActiveVersion);

  enum ResponseBodyType {
    UNKNOWN,
    BLOB,
    STREAM,
  };

  // We start processing the request if Start() is called AND response_type_
  // is determined.
  void MaybeStartRequest();
  void StartRequest();

  // Creates ServiceWorkerFetchRequest from |request_| and |body_|.
  std::unique_ptr<ServiceWorkerFetchRequest> CreateFetchRequest();

  // Creates BlobDataHandle of the request body from |body_|. This handle
  // |request_body_blob_data_handle_| will be deleted when
  // ServiceWorkerURLRequestJob is deleted.
  // This must not be called until all files in |body_| with unknown size have
  // their sizes populated.
  void CreateRequestBodyBlob(std::string* blob_uuid, uint64_t* blob_size);

  // Returns true if this job performed a navigation that should be logged to
  // performance-related UMA. It returns false in certain cases that are not
  // relevant to performance analysis, such as if the worker was not already
  // activated or DevTools was attached to the worker.
  bool ShouldRecordNavigationMetrics(const ServiceWorkerVersion* version) const;

  // For FORWARD_TO_SERVICE_WORKER case.
  void DidPrepareFetchEvent(scoped_refptr<ServiceWorkerVersion> version);
  void DidDispatchFetchEvent(
      ServiceWorkerStatusCode status,
      ServiceWorkerFetchEventResult fetch_result,
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      blink::mojom::BlobPtr body_as_blob,
      const scoped_refptr<ServiceWorkerVersion>& version);
  void SetResponse(const ServiceWorkerResponse& response);

  // Populates |http_response_headers_|.
  void CreateResponseHeader(int status_code,
                            const std::string& status_text,
                            const ServiceWorkerHeaderMap& headers);

  // Creates |http_response_info_| using |http_response_headers_| and calls
  // NotifyHeadersComplete.
  void CommitResponseHeader();

  // Creates and commits a response header indicating error.
  void DeliverErrorResponse();

  // Restarts this job to fallback to network.
  // This can be called after StartRequest.
  void FinalizeFallbackToNetwork();

  // Sends back a response with fall_back_required set as true to trigger
  // subsequent network requests with CORS checking.
  // This can be called after StartRequest.
  void FinalizeFallbackToRenderer();

  // True if need to send back a response with fall_back_required set as true to
  // trigger subsequent network requests with CORS checking.
  bool IsFallbackToRendererNeeded() const;

  // For UMA.
  void SetResponseBodyType(ResponseBodyType type);
  bool ShouldRecordResult();
  void RecordStatusZeroResponseError(
      blink::WebServiceWorkerResponseError error);

  const net::HttpResponseInfo* http_info() const;

  // Invoke callbacks before invoking corresponding URLRequestJob methods.
  void NotifyHeadersComplete();
  void NotifyStartError(net::URLRequestStatus status);
  void NotifyRestartRequired();

  // Wrapper that gathers parameters to |on_start_completed_callback_| and then
  // calls it.
  void OnStartCompleted() const;

  bool IsMainResourceLoad() const;

  // For waiting for files sizes of request body files with unknown sizes.
  bool HasRequestBody();
  void RequestBodyFileSizesResolved(bool success);

  // Called back from
  // ServiceWorkerFetchEventDispatcher::MaybeStartNavigationPreload when the
  // navigation preload response starts.
  void OnNavigationPreloadResponse();

  void MaybeReportNavigationPreloadMetrics();

  // Not owned.
  Delegate* delegate_;

  // Timing info to show on the popup in Devtools' Network tab.
  net::LoadTimingInfo load_timing_info_;

  // When the worker was asked to prepare for the fetch event. Preparation may
  // include activation and startup.
  base::TimeTicks worker_start_time_;

  // When the worker confirmed it's ready for the fetch event. If it was already
  // activated and running when asked to prepare, this should be nearly the same
  // as |worker_start_time_|).
  base::TimeTicks worker_ready_time_;

  // When the response started.
  base::Time response_time_;

  // When the navigation preload response started.
  base::TimeTicks navigation_preload_response_time_;

  // True if the worker was already in ACTIVATED status when asked to prepare
  // for the fetch event.
  bool worker_already_activated_ = false;

  // The status the worker was in when asked to prepare for the fetch event.
  EmbeddedWorkerStatus initial_worker_status_ = EmbeddedWorkerStatus::STOPPED;

  // If worker startup occurred during preparation, the situation that startup
  // occurred in.
  ServiceWorkerMetrics::StartSituation worker_start_situation_ =
      ServiceWorkerMetrics::StartSituation::UNKNOWN;

  // True if navigation preload was enabled.
  bool did_navigation_preload_ = false;
  bool simulate_navigation_preload_for_test_ = false;

  std::unique_ptr<NavigationPreloadMetrics> nav_preload_metrics_;

  ServiceWorkerResponseType response_type_;

  // True if URLRequestJob::Start() has been called.
  bool is_started_;

  net::HttpByteRange byte_range_;
  std::unique_ptr<net::HttpResponseInfo> range_response_info_;
  std::unique_ptr<net::HttpResponseInfo> http_response_info_;
  // Headers that have not yet been committed to |http_response_info_|.
  scoped_refptr<net::HttpResponseHeaders> http_response_headers_;
  std::vector<GURL> response_url_list_;
  network::mojom::FetchResponseType fetch_response_type_;

  // Used when response type is FORWARD_TO_SERVICE_WORKER.
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  std::string client_id_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  const ResourceContext* resource_context_;
  // Only one of |blob_reader_| and |data_pipe_reader_| can be non-null.
  std::unique_ptr<ServiceWorkerBlobReader> blob_reader_;
  std::unique_ptr<ServiceWorkerDataPipeReader> data_pipe_reader_;

  FetchRequestMode request_mode_;
  FetchCredentialsMode credentials_mode_;
  FetchRedirectMode redirect_mode_;
  std::string integrity_;
  const ResourceType resource_type_;
  RequestContextType request_context_type_;
  RequestContextFrameType frame_type_;
  bool fall_back_required_;
  // ResourceRequestBody has a collection of BlobDataHandles attached to it
  // using the userdata mechanism. So we have to keep it not to free the blobs.
  scoped_refptr<ResourceRequestBody> body_;
  std::unique_ptr<storage::BlobDataHandle> request_body_blob_data_handle_;
  scoped_refptr<storage::BlobHandle> request_body_blob_handle_;
  ServiceWorkerFetchType fetch_type_;
  base::Optional<base::TimeDelta> timeout_;

  ResponseBodyType response_body_type_ = UNKNOWN;
  bool did_record_result_ = false;

  bool response_is_in_cache_storage_ = false;
  std::string response_cache_storage_cache_name_;

  ServiceWorkerHeaderList cors_exposed_header_names_;

  std::unique_ptr<FileSizeResolver> file_size_resolver_;

  base::WeakPtrFactory<ServiceWorkerURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_REQUEST_JOB_H_
