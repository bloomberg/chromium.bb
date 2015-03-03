// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_cache_storage_dispatcher.h"

#include <map>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/service_worker/service_worker_script_context.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerResponse.h"

using base::TimeTicks;

namespace content {

using blink::WebServiceWorkerCacheError;
using blink::WebServiceWorkerRequest;

namespace {

ServiceWorkerFetchRequest FetchRequestFromWebRequest(
    const blink::WebServiceWorkerRequest& web_request) {
  ServiceWorkerHeaderMap headers;
  GetServiceWorkerHeaderMapFromWebRequest(web_request, &headers);

  return ServiceWorkerFetchRequest(
      web_request.url(), base::UTF16ToASCII(web_request.method()), headers,
      Referrer(web_request.referrerUrl(), web_request.referrerPolicy()),
      web_request.isReload());
}

void PopulateWebRequestFromFetchRequest(
    const ServiceWorkerFetchRequest& request,
    blink::WebServiceWorkerRequest* web_request) {
  web_request->setURL(request.url);
  web_request->setMethod(base::ASCIIToUTF16(request.method));
  for (ServiceWorkerHeaderMap::const_iterator i = request.headers.begin(),
                                            end = request.headers.end();
       i != end; ++i) {
    web_request->setHeader(base::ASCIIToUTF16(i->first),
                           base::ASCIIToUTF16(i->second));
  }
  web_request->setReferrer(base::ASCIIToUTF16(request.referrer.url.spec()),
                           request.referrer.policy);
  web_request->setIsReload(request.is_reload);
}

blink::WebVector<blink::WebServiceWorkerRequest> WebRequestsFromRequests(
    const std::vector<ServiceWorkerFetchRequest>& requests) {
  blink::WebVector<blink::WebServiceWorkerRequest>
      web_requests(requests.size());
  for (size_t i = 0; i < requests.size(); ++i)
    PopulateWebRequestFromFetchRequest(requests[i], &(web_requests[i]));
  return web_requests;
}

ServiceWorkerResponse ResponseFromWebResponse(
    const blink::WebServiceWorkerResponse& web_response) {
  ServiceWorkerHeaderMap headers;
  GetServiceWorkerHeaderMapFromWebResponse(web_response, &headers);
  // We don't support streaming for cache.
  DCHECK(web_response.streamURL().isEmpty());
  return ServiceWorkerResponse(web_response.url(),
                               web_response.status(),
                               base::UTF16ToASCII(web_response.statusText()),
                               web_response.responseType(),
                               headers,
                               base::UTF16ToASCII(web_response.blobUUID()),
                               web_response.blobSize(),
                               web_response.streamURL());
}

ServiceWorkerCacheQueryParams QueryParamsFromWebQueryParams(
    const blink::WebServiceWorkerCache::QueryParams& web_query_params) {
  ServiceWorkerCacheQueryParams query_params;
  query_params.ignore_search = web_query_params.ignoreSearch;
  query_params.ignore_method = web_query_params.ignoreMethod;
  query_params.ignore_vary = web_query_params.ignoreVary;
  query_params.cache_name = web_query_params.cacheName;
  return query_params;
}

ServiceWorkerCacheOperationType CacheOperationTypeFromWebCacheOperationType(
    blink::WebServiceWorkerCache::OperationType operation_type) {
  switch (operation_type) {
    case blink::WebServiceWorkerCache::OperationTypePut:
      return SERVICE_WORKER_CACHE_OPERATION_TYPE_PUT;
    case blink::WebServiceWorkerCache::OperationTypeDelete:
      return SERVICE_WORKER_CACHE_OPERATION_TYPE_DELETE;
    default:
      return SERVICE_WORKER_CACHE_OPERATION_TYPE_UNDEFINED;
  }
}

ServiceWorkerBatchOperation BatchOperationFromWebBatchOperation(
    const blink::WebServiceWorkerCache::BatchOperation& web_operation) {
  ServiceWorkerBatchOperation operation;
  operation.operation_type =
      CacheOperationTypeFromWebCacheOperationType(web_operation.operationType);
  operation.request = FetchRequestFromWebRequest(web_operation.request);
  operation.response = ResponseFromWebResponse(web_operation.response);
  operation.match_params =
      QueryParamsFromWebQueryParams(web_operation.matchParams);
  return operation;
}

template<typename T>
void ClearCallbacksMapWithErrors(T* callbacks_map) {
  typename T::iterator iter(callbacks_map);
  while (!iter.IsAtEnd()) {
    blink::WebServiceWorkerCacheError reason =
        blink::WebServiceWorkerCacheErrorNotFound;
    iter.GetCurrentValue()->onError(&reason);
    callbacks_map->Remove(iter.GetCurrentKey());
    iter.Advance();
  }
}

}  // namespace

// The WebCache object is the Chromium side implementation of the Blink
// WebServiceWorkerCache API. Most of its methods delegate directly to the
// ServiceWorkerStorage object, which is able to assign unique IDs as well
// as have a lifetime longer than the requests.
class ServiceWorkerCacheStorageDispatcher::WebCache
    : public blink::WebServiceWorkerCache {
 public:
  WebCache(base::WeakPtr<ServiceWorkerCacheStorageDispatcher> dispatcher,
           int cache_id)
      : dispatcher_(dispatcher),
        cache_id_(cache_id) {}

  virtual ~WebCache() {
    if (dispatcher_)
      dispatcher_->OnWebCacheDestruction(cache_id_);
  }

  // From blink::WebServiceWorkerCache:
  virtual void dispatchMatch(CacheMatchCallbacks* callbacks,
                             const blink::WebServiceWorkerRequest& request,
                             const QueryParams& query_params) {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchMatchForCache(cache_id_, callbacks, request,
                                       query_params);
  }
  virtual void dispatchMatchAll(CacheWithResponsesCallbacks* callbacks,
                                const blink::WebServiceWorkerRequest& request,
                                const QueryParams& query_params) {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchMatchAllForCache(cache_id_, callbacks, request,
                                          query_params);
  }
  virtual void dispatchKeys(CacheWithRequestsCallbacks* callbacks,
                            const blink::WebServiceWorkerRequest* request,
                            const QueryParams& query_params) {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchKeysForCache(cache_id_, callbacks, request,
                                      query_params);
  }
  virtual void dispatchBatch(
      CacheWithResponsesCallbacks* callbacks,
      const blink::WebVector<BatchOperation>& batch_operations) {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchBatchForCache(cache_id_, callbacks, batch_operations);
  }

 private:
  const base::WeakPtr<ServiceWorkerCacheStorageDispatcher> dispatcher_;
  const int cache_id_;
};

ServiceWorkerCacheStorageDispatcher::ServiceWorkerCacheStorageDispatcher(
    ServiceWorkerScriptContext* script_context)
    : script_context_(script_context),
      weak_factory_(this) {}

ServiceWorkerCacheStorageDispatcher::~ServiceWorkerCacheStorageDispatcher() {
  ClearCallbacksMapWithErrors(&has_callbacks_);
  ClearCallbacksMapWithErrors(&open_callbacks_);
  ClearCallbacksMapWithErrors(&delete_callbacks_);
  ClearCallbacksMapWithErrors(&keys_callbacks_);
  ClearCallbacksMapWithErrors(&match_callbacks_);

  ClearCallbacksMapWithErrors(&cache_match_callbacks_);
  ClearCallbacksMapWithErrors(&cache_match_all_callbacks_);
  ClearCallbacksMapWithErrors(&cache_keys_callbacks_);
  ClearCallbacksMapWithErrors(&cache_batch_callbacks_);
}

bool ServiceWorkerCacheStorageDispatcher::OnMessageReceived(
    const IPC::Message& message)  {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerCacheStorageDispatcher, message)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageHasSuccess,
                          OnCacheStorageHasSuccess)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageOpenSuccess,
                          OnCacheStorageOpenSuccess)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageDeleteSuccess,
                          OnCacheStorageDeleteSuccess)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageKeysSuccess,
                          OnCacheStorageKeysSuccess)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageMatchSuccess,
                          OnCacheStorageMatchSuccess)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageHasError,
                          OnCacheStorageHasError)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageOpenError,
                          OnCacheStorageOpenError)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageDeleteError,
                          OnCacheStorageDeleteError)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageKeysError,
                          OnCacheStorageKeysError)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageMatchError,
                          OnCacheStorageMatchError)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheMatchSuccess,
                          OnCacheMatchSuccess)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheMatchAllSuccess,
                          OnCacheMatchAllSuccess)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheKeysSuccess,
                          OnCacheKeysSuccess)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheBatchSuccess,
                          OnCacheBatchSuccess)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheMatchError,
                          OnCacheMatchError)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheMatchAllError,
                          OnCacheMatchAllError)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheKeysError,
                          OnCacheKeysError)
      IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheBatchError,
                          OnCacheBatchError)
     IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageHasSuccess(
    int request_id) {
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Has",
                      TimeTicks::Now() - has_times_[request_id]);
  CacheStorageCallbacks* callbacks = has_callbacks_.Lookup(request_id);
  callbacks->onSuccess();
  has_callbacks_.Remove(request_id);
  has_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageOpenSuccess(
    int request_id,
    int cache_id) {
  WebCache* web_cache = new WebCache(weak_factory_.GetWeakPtr(), cache_id);
  web_caches_.AddWithID(web_cache, cache_id);
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Open",
                      TimeTicks::Now() - open_times_[request_id]);
  CacheStorageWithCacheCallbacks* callbacks =
      open_callbacks_.Lookup(request_id);
  callbacks->onSuccess(web_cache);
  open_callbacks_.Remove(request_id);
  open_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageDeleteSuccess(
    int request_id) {
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Delete",
                      TimeTicks::Now() - delete_times_[request_id]);
  CacheStorageCallbacks* callbacks = delete_callbacks_.Lookup(request_id);
  callbacks->onSuccess();
  delete_callbacks_.Remove(request_id);
  delete_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageKeysSuccess(
    int request_id,
    const std::vector<base::string16>& keys) {
  blink::WebVector<blink::WebString> webKeys(keys.size());
  for (size_t i = 0; i < keys.size(); ++i)
    webKeys[i] = keys[i];

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Keys",
                      TimeTicks::Now() - keys_times_[request_id]);
  CacheStorageKeysCallbacks* callbacks = keys_callbacks_.Lookup(request_id);
  callbacks->onSuccess(&webKeys);
  keys_callbacks_.Remove(request_id);
  keys_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageMatchSuccess(
    int request_id,
    const ServiceWorkerResponse& response) {
  blink::WebServiceWorkerResponse web_response;
  PopulateWebResponseFromResponse(response, &web_response);

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Match",
                      TimeTicks::Now() - match_times_[request_id]);
  CacheStorageMatchCallbacks* callbacks = match_callbacks_.Lookup(request_id);
  callbacks->onSuccess(&web_response);
  match_callbacks_.Remove(request_id);
  match_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageHasError(
      int request_id,
      blink::WebServiceWorkerCacheError reason) {
  CacheStorageCallbacks* callbacks = has_callbacks_.Lookup(request_id);
  callbacks->onError(&reason);
  has_callbacks_.Remove(request_id);
  has_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageOpenError(
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  CacheStorageWithCacheCallbacks* callbacks =
      open_callbacks_.Lookup(request_id);
  callbacks->onError(&reason);
  open_callbacks_.Remove(request_id);
  open_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageDeleteError(
      int request_id,
      blink::WebServiceWorkerCacheError reason) {
  CacheStorageCallbacks* callbacks = delete_callbacks_.Lookup(request_id);
  callbacks->onError(&reason);
  delete_callbacks_.Remove(request_id);
  delete_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageKeysError(
      int request_id,
      blink::WebServiceWorkerCacheError reason) {
  CacheStorageKeysCallbacks* callbacks = keys_callbacks_.Lookup(request_id);
  callbacks->onError(&reason);
  keys_callbacks_.Remove(request_id);
  keys_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageMatchError(
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  CacheStorageMatchCallbacks* callbacks = match_callbacks_.Lookup(request_id);
  callbacks->onError(&reason);
  match_callbacks_.Remove(request_id);
  match_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheMatchSuccess(
    int request_id,
    const ServiceWorkerResponse& response) {
  blink::WebServiceWorkerResponse web_response;
  PopulateWebResponseFromResponse(response, &web_response);

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Match",
                      TimeTicks::Now() - cache_match_times_[request_id]);
  blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks =
      cache_match_callbacks_.Lookup(request_id);
  callbacks->onSuccess(&web_response);
  cache_match_callbacks_.Remove(request_id);
  cache_match_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheMatchAllSuccess(
    int request_id,
    const std::vector<ServiceWorkerResponse>& responses) {
  blink::WebVector<blink::WebServiceWorkerResponse>
      web_responses = WebResponsesFromResponses(responses);

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.MatchAll",
                      TimeTicks::Now() - cache_match_all_times_[request_id]);
  blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks =
      cache_match_all_callbacks_.Lookup(request_id);
  callbacks->onSuccess(&web_responses);
  cache_match_all_callbacks_.Remove(request_id);
  cache_match_all_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheKeysSuccess(
    int request_id,
    const std::vector<ServiceWorkerFetchRequest>& requests) {
  blink::WebVector<blink::WebServiceWorkerRequest>
      web_requests = WebRequestsFromRequests(requests);

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Keys",
                      TimeTicks::Now() - cache_keys_times_[request_id]);
  blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks =
      cache_keys_callbacks_.Lookup(request_id);
  callbacks->onSuccess(&web_requests);
  cache_keys_callbacks_.Remove(request_id);
  cache_keys_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheBatchSuccess(
    int request_id,
    const std::vector<ServiceWorkerResponse>& responses) {
  blink::WebVector<blink::WebServiceWorkerResponse>
      web_responses = WebResponsesFromResponses(responses);

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Batch",
                      TimeTicks::Now() - cache_batch_times_[request_id]);
  blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks =
      cache_batch_callbacks_.Lookup(request_id);
  callbacks->onSuccess(&web_responses);
  cache_batch_callbacks_.Remove(request_id);
  cache_batch_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheMatchError(
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks =
      cache_match_callbacks_.Lookup(request_id);
  callbacks->onError(&reason);
  cache_match_callbacks_.Remove(request_id);
  cache_match_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheMatchAllError(
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks =
      cache_match_all_callbacks_.Lookup(request_id);
  callbacks->onError(&reason);
  cache_match_all_callbacks_.Remove(request_id);
  cache_match_all_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheKeysError(
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks =
      cache_keys_callbacks_.Lookup(request_id);
  callbacks->onError(&reason);
  cache_keys_callbacks_.Remove(request_id);
  cache_keys_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheBatchError(
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks =
      cache_batch_callbacks_.Lookup(request_id);
  callbacks->onError(&reason);
  cache_batch_callbacks_.Remove(request_id);
  cache_batch_times_.erase(request_id);
}

void ServiceWorkerCacheStorageDispatcher::dispatchHas(
    CacheStorageCallbacks* callbacks,
    const blink::WebString& cacheName) {
  int request_id = has_callbacks_.Add(callbacks);
  has_times_[request_id] = base::TimeTicks::Now();
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageHas(
      script_context_->GetRoutingID(), request_id, cacheName));
}

void ServiceWorkerCacheStorageDispatcher::dispatchOpen(
    CacheStorageWithCacheCallbacks* callbacks,
    const blink::WebString& cacheName) {
  int request_id = open_callbacks_.Add(callbacks);
  open_times_[request_id] = base::TimeTicks::Now();
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageOpen(
      script_context_->GetRoutingID(), request_id, cacheName));
}

void ServiceWorkerCacheStorageDispatcher::dispatchDelete(
    CacheStorageCallbacks* callbacks,
    const blink::WebString& cacheName) {
  int request_id = delete_callbacks_.Add(callbacks);
  delete_times_[request_id] = base::TimeTicks::Now();
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageDelete(
      script_context_->GetRoutingID(), request_id, cacheName));
}

void ServiceWorkerCacheStorageDispatcher::dispatchKeys(
    CacheStorageKeysCallbacks* callbacks) {
  int request_id = keys_callbacks_.Add(callbacks);
  keys_times_[request_id] = base::TimeTicks::Now();
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageKeys(
      script_context_->GetRoutingID(), request_id));
}

void ServiceWorkerCacheStorageDispatcher::dispatchMatch(
    CacheStorageMatchCallbacks* callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = match_callbacks_.Add(callbacks);
  match_times_[request_id] = base::TimeTicks::Now();
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageMatch(
      script_context_->GetRoutingID(), request_id,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void ServiceWorkerCacheStorageDispatcher::dispatchMatchForCache(
    int cache_id,
    blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = cache_match_callbacks_.Add(callbacks);
  cache_match_times_[request_id] = base::TimeTicks::Now();

  script_context_->Send(new ServiceWorkerHostMsg_CacheMatch(
      script_context_->GetRoutingID(), request_id, cache_id,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void ServiceWorkerCacheStorageDispatcher::dispatchMatchAllForCache(
    int cache_id,
    blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = cache_match_all_callbacks_.Add(callbacks);
  cache_match_all_times_[request_id] = base::TimeTicks::Now();

  script_context_->Send(new ServiceWorkerHostMsg_CacheMatchAll(
      script_context_->GetRoutingID(), request_id, cache_id,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void ServiceWorkerCacheStorageDispatcher::dispatchKeysForCache(
    int cache_id,
    blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks,
    const blink::WebServiceWorkerRequest* request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = cache_keys_callbacks_.Add(callbacks);
  cache_keys_times_[request_id] = base::TimeTicks::Now();

  script_context_->Send(new ServiceWorkerHostMsg_CacheKeys(
      script_context_->GetRoutingID(), request_id, cache_id,
      request ? FetchRequestFromWebRequest(*request)
              : ServiceWorkerFetchRequest(),
      QueryParamsFromWebQueryParams(query_params)));
}

void ServiceWorkerCacheStorageDispatcher::dispatchBatchForCache(
    int cache_id,
    blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks,
    const blink::WebVector<
        blink::WebServiceWorkerCache::BatchOperation>& web_operations) {
  int request_id = cache_batch_callbacks_.Add(callbacks);
  cache_batch_times_[request_id] = base::TimeTicks::Now();

  std::vector<ServiceWorkerBatchOperation> operations;
  operations.reserve(web_operations.size());
  for (size_t i = 0; i < web_operations.size(); ++i) {
    operations.push_back(
        BatchOperationFromWebBatchOperation(web_operations[i]));
  }

  script_context_->Send(new ServiceWorkerHostMsg_CacheBatch(
      script_context_->GetRoutingID(), request_id, cache_id, operations));
}

void ServiceWorkerCacheStorageDispatcher::OnWebCacheDestruction(int cache_id) {
  web_caches_.Remove(cache_id);
  script_context_->Send(new ServiceWorkerHostMsg_CacheClosed(
      script_context_->GetRoutingID(), cache_id));
}

void ServiceWorkerCacheStorageDispatcher::PopulateWebResponseFromResponse(
    const ServiceWorkerResponse& response,
    blink::WebServiceWorkerResponse* web_response) {
  web_response->setURL(response.url);
  web_response->setStatus(response.status_code);
  web_response->setStatusText(base::ASCIIToUTF16(response.status_text));
  web_response->setResponseType(response.response_type);

  for (const auto& i : response.headers) {
    web_response->setHeader(base::ASCIIToUTF16(i.first),
                            base::ASCIIToUTF16(i.second));
  }

  if (!response.blob_uuid.empty()) {
    web_response->setBlob(blink::WebString::fromUTF8(response.blob_uuid),
                          response.blob_size);
    // Let the host know that it can release its reference to the blob.
    script_context_->Send(new ServiceWorkerHostMsg_BlobDataHandled(
        script_context_->GetRoutingID(), response.blob_uuid));
  }
}

blink::WebVector<blink::WebServiceWorkerResponse>
ServiceWorkerCacheStorageDispatcher::WebResponsesFromResponses(
    const std::vector<ServiceWorkerResponse>& responses) {
  blink::WebVector<blink::WebServiceWorkerResponse> web_responses(
      responses.size());
  for (size_t i = 0; i < responses.size(); ++i)
    PopulateWebResponseFromResponse(responses[i], &(web_responses[i]));
  return web_responses;
}

}  // namespace content
