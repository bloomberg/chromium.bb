// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/cache_storage/cache_storage_dispatcher.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/cache_storage/cache_storage_messages.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "url/origin.h"

using base::TimeTicks;

namespace content {

using blink::WebServiceWorkerCacheError;
using blink::WebServiceWorkerCacheStorage;
using blink::WebServiceWorkerRequest;
using blink::WebString;

static base::LazyInstance<base::ThreadLocalPointer<CacheStorageDispatcher>>::
    Leaky g_cache_storage_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

namespace {

CacheStorageDispatcher* const kHasBeenDeleted =
    reinterpret_cast<CacheStorageDispatcher*>(0x1);

ServiceWorkerFetchRequest FetchRequestFromWebRequest(
    const blink::WebServiceWorkerRequest& web_request) {
  ServiceWorkerHeaderMap headers;
  GetServiceWorkerHeaderMapFromWebRequest(web_request, &headers);

  return ServiceWorkerFetchRequest(
      web_request.Url(), web_request.Method().Ascii(), headers,
      Referrer(web_request.ReferrerUrl(), web_request.GetReferrerPolicy()),
      web_request.IsReload());
}

void PopulateWebRequestFromFetchRequest(
    const ServiceWorkerFetchRequest& request,
    blink::WebServiceWorkerRequest* web_request) {
  web_request->SetURL(request.url);
  web_request->SetMethod(WebString::FromASCII(request.method));
  for (ServiceWorkerHeaderMap::const_iterator i = request.headers.begin(),
                                              end = request.headers.end();
       i != end; ++i) {
    web_request->SetHeader(WebString::FromASCII(i->first),
                           WebString::FromASCII(i->second));
  }
  web_request->SetReferrer(WebString::FromASCII(request.referrer.url.spec()),
                           request.referrer.policy);
  web_request->SetIsReload(request.is_reload);
}

blink::WebVector<blink::WebServiceWorkerRequest> WebRequestsFromRequests(
    const std::vector<ServiceWorkerFetchRequest>& requests) {
  blink::WebVector<blink::WebServiceWorkerRequest> web_requests(
      requests.size());
  for (size_t i = 0; i < requests.size(); ++i)
    PopulateWebRequestFromFetchRequest(requests[i], &(web_requests[i]));
  return web_requests;
}

CacheStorageCacheQueryParams QueryParamsFromWebQueryParams(
    const blink::WebServiceWorkerCache::QueryParams& web_query_params) {
  CacheStorageCacheQueryParams query_params;
  query_params.ignore_search = web_query_params.ignore_search;
  query_params.ignore_method = web_query_params.ignore_method;
  query_params.ignore_vary = web_query_params.ignore_vary;
  query_params.cache_name =
      blink::WebString::ToNullableString16(web_query_params.cache_name);
  return query_params;
}

CacheStorageCacheOperationType CacheOperationTypeFromWebCacheOperationType(
    blink::WebServiceWorkerCache::OperationType operation_type) {
  switch (operation_type) {
    case blink::WebServiceWorkerCache::kOperationTypePut:
      return CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
    case blink::WebServiceWorkerCache::kOperationTypeDelete:
      return CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE;
    default:
      return CACHE_STORAGE_CACHE_OPERATION_TYPE_UNDEFINED;
  }
}

CacheStorageBatchOperation BatchOperationFromWebBatchOperation(
    const blink::WebServiceWorkerCache::BatchOperation& web_operation) {
  CacheStorageBatchOperation operation;
  operation.operation_type =
      CacheOperationTypeFromWebCacheOperationType(web_operation.operation_type);
  operation.request = FetchRequestFromWebRequest(web_operation.request);
  operation.response =
      GetServiceWorkerResponseFromWebResponse(web_operation.response);
  operation.match_params =
      QueryParamsFromWebQueryParams(web_operation.match_params);
  return operation;
}

template <typename T>
void ClearCallbacksMapWithErrors(T* callbacks_map) {
  typename T::iterator iter(callbacks_map);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->OnError(blink::kWebServiceWorkerCacheErrorNotFound);
    callbacks_map->Remove(iter.GetCurrentKey());
    iter.Advance();
  }
}

}  // namespace

// The WebCache object is the Chromium side implementation of the Blink
// WebServiceWorkerCache API. Most of its methods delegate directly to the
// ServiceWorkerStorage object, which is able to assign unique IDs as well
// as have a lifetime longer than the requests.
class CacheStorageDispatcher::WebCache : public blink::WebServiceWorkerCache {
 public:
  WebCache(base::WeakPtr<CacheStorageDispatcher> dispatcher, int cache_id)
      : dispatcher_(dispatcher), cache_id_(cache_id) {}

  ~WebCache() override {
    if (dispatcher_)
      dispatcher_->OnWebCacheDestruction(cache_id_);
  }

  // From blink::WebServiceWorkerCache:
  void DispatchMatch(std::unique_ptr<CacheMatchCallbacks> callbacks,
                     const blink::WebServiceWorkerRequest& request,
                     const QueryParams& query_params) override {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchMatchForCache(cache_id_, std::move(callbacks), request,
                                       query_params);
  }
  void DispatchMatchAll(std::unique_ptr<CacheWithResponsesCallbacks> callbacks,
                        const blink::WebServiceWorkerRequest& request,
                        const QueryParams& query_params) override {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchMatchAllForCache(cache_id_, std::move(callbacks),
                                          request, query_params);
  }
  void DispatchKeys(std::unique_ptr<CacheWithRequestsCallbacks> callbacks,
                    const blink::WebServiceWorkerRequest& request,
                    const QueryParams& query_params) override {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchKeysForCache(cache_id_, std::move(callbacks), request,
                                      query_params);
  }
  void DispatchBatch(
      std::unique_ptr<CacheBatchCallbacks> callbacks,
      const blink::WebVector<BatchOperation>& batch_operations) override {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchBatchForCache(cache_id_, std::move(callbacks),
                                       batch_operations);
  }

 private:
  const base::WeakPtr<CacheStorageDispatcher> dispatcher_;
  const int cache_id_;
};

CacheStorageDispatcher::CacheStorageDispatcher(
    ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender), weak_factory_(this) {
  g_cache_storage_dispatcher_tls.Pointer()->Set(this);
}

CacheStorageDispatcher::~CacheStorageDispatcher() {
  ClearCallbacksMapWithErrors(&has_callbacks_);
  ClearCallbacksMapWithErrors(&open_callbacks_);
  ClearCallbacksMapWithErrors(&delete_callbacks_);
  ClearCallbacksMapWithErrors(&keys_callbacks_);
  ClearCallbacksMapWithErrors(&match_callbacks_);

  ClearCallbacksMapWithErrors(&cache_match_callbacks_);
  ClearCallbacksMapWithErrors(&cache_match_all_callbacks_);
  ClearCallbacksMapWithErrors(&cache_keys_callbacks_);
  ClearCallbacksMapWithErrors(&cache_batch_callbacks_);

  g_cache_storage_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

CacheStorageDispatcher* CacheStorageDispatcher::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender) {
  if (g_cache_storage_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS CacheStorageDispatcher.";
    g_cache_storage_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_cache_storage_dispatcher_tls.Pointer()->Get())
    return g_cache_storage_dispatcher_tls.Pointer()->Get();

  CacheStorageDispatcher* dispatcher =
      new CacheStorageDispatcher(thread_safe_sender);
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(dispatcher);
  return dispatcher;
}

void CacheStorageDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

bool CacheStorageDispatcher::Send(IPC::Message* msg) {
  return thread_safe_sender_->Send(msg);
}

bool CacheStorageDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CacheStorageDispatcher, message)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageHasSuccess,
                          OnCacheStorageHasSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageOpenSuccess,
                          OnCacheStorageOpenSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageDeleteSuccess,
                          OnCacheStorageDeleteSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageKeysSuccess,
                          OnCacheStorageKeysSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageMatchSuccess,
                          OnCacheStorageMatchSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageHasError,
                          OnCacheStorageHasError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageOpenError,
                          OnCacheStorageOpenError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageDeleteError,
                          OnCacheStorageDeleteError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageMatchError,
                          OnCacheStorageMatchError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheMatchSuccess,
                          OnCacheMatchSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheMatchAllSuccess,
                          OnCacheMatchAllSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheKeysSuccess, OnCacheKeysSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheBatchSuccess,
                          OnCacheBatchSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheMatchError, OnCacheMatchError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheMatchAllError,
                          OnCacheMatchAllError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheKeysError, OnCacheKeysError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheBatchError, OnCacheBatchError)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CacheStorageDispatcher::OnCacheStorageHasSuccess(int thread_id,
                                                      int request_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Has",
                      TimeTicks::Now() - has_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks =
      has_callbacks_.Lookup(request_id);
  callbacks->OnSuccess();
  has_callbacks_.Remove(request_id);
  has_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageOpenSuccess(int thread_id,
                                                       int request_id,
                                                       int cache_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  std::unique_ptr<WebCache> web_cache(
      new WebCache(weak_factory_.GetWeakPtr(), cache_id));
  web_caches_.AddWithID(web_cache.get(), cache_id);
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Open",
                      TimeTicks::Now() - open_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks* callbacks =
      open_callbacks_.Lookup(request_id);
  callbacks->OnSuccess(std::move(web_cache));
  open_callbacks_.Remove(request_id);
  open_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageDeleteSuccess(int thread_id,
                                                         int request_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Delete",
                      TimeTicks::Now() - delete_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks =
      delete_callbacks_.Lookup(request_id);
  callbacks->OnSuccess();
  delete_callbacks_.Remove(request_id);
  delete_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageKeysSuccess(
    int thread_id,
    int request_id,
    const std::vector<base::string16>& keys) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebVector<blink::WebString> web_keys(keys.size());
  std::transform(
      keys.begin(), keys.end(), web_keys.begin(),
      [](const base::string16& s) { return WebString::FromUTF16(s); });
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Keys",
                      TimeTicks::Now() - keys_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks* callbacks =
      keys_callbacks_.Lookup(request_id);
  callbacks->OnSuccess(web_keys);
  keys_callbacks_.Remove(request_id);
  keys_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageMatchSuccess(
    int thread_id,
    int request_id,
    const ServiceWorkerResponse& response) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerResponse web_response;
  PopulateWebResponseFromResponse(response, &web_response);

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Match",
                      TimeTicks::Now() - match_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks* callbacks =
      match_callbacks_.Lookup(request_id);
  callbacks->OnSuccess(web_response);
  match_callbacks_.Remove(request_id);
  match_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageHasError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks =
      has_callbacks_.Lookup(request_id);
  callbacks->OnError(reason);
  has_callbacks_.Remove(request_id);
  has_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageOpenError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks* callbacks =
      open_callbacks_.Lookup(request_id);
  callbacks->OnError(reason);
  open_callbacks_.Remove(request_id);
  open_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageDeleteError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks =
      delete_callbacks_.Lookup(request_id);
  callbacks->OnError(reason);
  delete_callbacks_.Remove(request_id);
  delete_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageMatchError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks* callbacks =
      match_callbacks_.Lookup(request_id);
  callbacks->OnError(reason);
  match_callbacks_.Remove(request_id);
  match_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheMatchSuccess(
    int thread_id,
    int request_id,
    const ServiceWorkerResponse& response) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerResponse web_response;
  PopulateWebResponseFromResponse(response, &web_response);

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Match",
                      TimeTicks::Now() - cache_match_times_[request_id]);
  blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks =
      cache_match_callbacks_.Lookup(request_id);
  callbacks->OnSuccess(web_response);
  cache_match_callbacks_.Remove(request_id);
  cache_match_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheMatchAllSuccess(
    int thread_id,
    int request_id,
    const std::vector<ServiceWorkerResponse>& responses) {
  DCHECK_EQ(thread_id, CurrentWorkerId());

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.MatchAll",
                      TimeTicks::Now() - cache_match_all_times_[request_id]);
  blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks =
      cache_match_all_callbacks_.Lookup(request_id);
  callbacks->OnSuccess(WebResponsesFromResponses(responses));
  cache_match_all_callbacks_.Remove(request_id);
  cache_match_all_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheKeysSuccess(
    int thread_id,
    int request_id,
    const std::vector<ServiceWorkerFetchRequest>& requests) {
  DCHECK_EQ(thread_id, CurrentWorkerId());

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Keys",
                      TimeTicks::Now() - cache_keys_times_[request_id]);
  blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks =
      cache_keys_callbacks_.Lookup(request_id);
  callbacks->OnSuccess(WebRequestsFromRequests(requests));
  cache_keys_callbacks_.Remove(request_id);
  cache_keys_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheBatchSuccess(
    int thread_id,
    int request_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Batch",
                      TimeTicks::Now() - cache_batch_times_[request_id]);
  blink::WebServiceWorkerCache::CacheBatchCallbacks* callbacks =
      cache_batch_callbacks_.Lookup(request_id);
  callbacks->OnSuccess();
  cache_batch_callbacks_.Remove(request_id);
  cache_batch_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheMatchError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks =
      cache_match_callbacks_.Lookup(request_id);
  callbacks->OnError(reason);
  cache_match_callbacks_.Remove(request_id);
  cache_match_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheMatchAllError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks =
      cache_match_all_callbacks_.Lookup(request_id);
  callbacks->OnError(reason);
  cache_match_all_callbacks_.Remove(request_id);
  cache_match_all_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheKeysError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks =
      cache_keys_callbacks_.Lookup(request_id);
  callbacks->OnError(reason);
  cache_keys_callbacks_.Remove(request_id);
  cache_keys_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheBatchError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerCache::CacheBatchCallbacks* callbacks =
      cache_batch_callbacks_.Lookup(request_id);
  callbacks->OnError(blink::WebServiceWorkerCacheError(reason));
  cache_batch_callbacks_.Remove(request_id);
  cache_batch_times_.erase(request_id);
}

void CacheStorageDispatcher::dispatchHas(
    std::unique_ptr<WebServiceWorkerCacheStorage::CacheStorageCallbacks>
        callbacks,
    const url::Origin& origin,
    const blink::WebString& cacheName) {
  int request_id = has_callbacks_.Add(std::move(callbacks));
  has_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageHas(CurrentWorkerId(), request_id,
                                               origin, cacheName.Utf16()));
}

void CacheStorageDispatcher::dispatchOpen(
    std::unique_ptr<
        WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks> callbacks,
    const url::Origin& origin,
    const blink::WebString& cacheName) {
  int request_id = open_callbacks_.Add(std::move(callbacks));
  open_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageOpen(CurrentWorkerId(), request_id,
                                                origin, cacheName.Utf16()));
}

void CacheStorageDispatcher::dispatchDelete(
    std::unique_ptr<WebServiceWorkerCacheStorage::CacheStorageCallbacks>
        callbacks,
    const url::Origin& origin,
    const blink::WebString& cacheName) {
  int request_id = delete_callbacks_.Add(std::move(callbacks));
  delete_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageDelete(CurrentWorkerId(), request_id,
                                                  origin, cacheName.Utf16()));
}

void CacheStorageDispatcher::dispatchKeys(
    std::unique_ptr<WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks>
        callbacks,
    const url::Origin& origin) {
  int request_id = keys_callbacks_.Add(std::move(callbacks));
  keys_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageKeys(CurrentWorkerId(), request_id,
                                                origin));
}

void CacheStorageDispatcher::dispatchMatch(
    std::unique_ptr<WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks>
        callbacks,
    const url::Origin& origin,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = match_callbacks_.Add(std::move(callbacks));
  match_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageMatch(
      CurrentWorkerId(), request_id, origin,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void CacheStorageDispatcher::dispatchMatchForCache(
    int cache_id,
    std::unique_ptr<blink::WebServiceWorkerCache::CacheMatchCallbacks>
        callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = cache_match_callbacks_.Add(std::move(callbacks));
  cache_match_times_[request_id] = base::TimeTicks::Now();

  Send(new CacheStorageHostMsg_CacheMatch(
      CurrentWorkerId(), request_id, cache_id,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void CacheStorageDispatcher::dispatchMatchAllForCache(
    int cache_id,
    std::unique_ptr<blink::WebServiceWorkerCache::CacheWithResponsesCallbacks>
        callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = cache_match_all_callbacks_.Add(std::move(callbacks));
  cache_match_all_times_[request_id] = base::TimeTicks::Now();

  Send(new CacheStorageHostMsg_CacheMatchAll(
      CurrentWorkerId(), request_id, cache_id,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void CacheStorageDispatcher::dispatchKeysForCache(
    int cache_id,
    std::unique_ptr<blink::WebServiceWorkerCache::CacheWithRequestsCallbacks>
        callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = cache_keys_callbacks_.Add(std::move(callbacks));
  cache_keys_times_[request_id] = base::TimeTicks::Now();

  Send(new CacheStorageHostMsg_CacheKeys(
      CurrentWorkerId(), request_id, cache_id,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void CacheStorageDispatcher::dispatchBatchForCache(
    int cache_id,
    std::unique_ptr<blink::WebServiceWorkerCache::CacheBatchCallbacks>
        callbacks,
    const blink::WebVector<blink::WebServiceWorkerCache::BatchOperation>&
        web_operations) {
  int request_id = cache_batch_callbacks_.Add(std::move(callbacks));
  cache_batch_times_[request_id] = base::TimeTicks::Now();

  std::vector<CacheStorageBatchOperation> operations;
  operations.reserve(web_operations.size());
  for (size_t i = 0; i < web_operations.size(); ++i) {
    operations.push_back(
        BatchOperationFromWebBatchOperation(web_operations[i]));
  }

  Send(new CacheStorageHostMsg_CacheBatch(CurrentWorkerId(), request_id,
                                          cache_id, operations));
}

void CacheStorageDispatcher::OnWebCacheDestruction(int cache_id) {
  web_caches_.Remove(cache_id);
  Send(new CacheStorageHostMsg_CacheClosed(cache_id));
}

void CacheStorageDispatcher::PopulateWebResponseFromResponse(
    const ServiceWorkerResponse& response,
    blink::WebServiceWorkerResponse* web_response) {
  web_response->SetURLList(response.url_list);
  web_response->SetStatus(response.status_code);
  web_response->SetStatusText(WebString::FromASCII(response.status_text));
  web_response->SetResponseType(response.response_type);
  web_response->SetResponseTime(response.response_time);
  web_response->SetCacheStorageCacheName(
      response.is_in_cache_storage
          ? blink::WebString::FromUTF8(response.cache_storage_cache_name)
          : blink::WebString());
  blink::WebVector<blink::WebString> headers(
      response.cors_exposed_header_names.size());
  std::transform(response.cors_exposed_header_names.begin(),
                 response.cors_exposed_header_names.end(), headers.begin(),
                 [](const std::string& s) { return WebString::FromLatin1(s); });
  web_response->SetCorsExposedHeaderNames(headers);

  for (const auto& i : response.headers) {
    web_response->SetHeader(WebString::FromASCII(i.first),
                            WebString::FromASCII(i.second));
  }

  if (!response.blob_uuid.empty()) {
    web_response->SetBlob(blink::WebString::FromUTF8(response.blob_uuid),
                          response.blob_size);
    // Let the host know that it can release its reference to the blob.
    Send(new CacheStorageHostMsg_BlobDataHandled(response.blob_uuid));
  }
}

blink::WebVector<blink::WebServiceWorkerResponse>
CacheStorageDispatcher::WebResponsesFromResponses(
    const std::vector<ServiceWorkerResponse>& responses) {
  blink::WebVector<blink::WebServiceWorkerResponse> web_responses(
      responses.size());
  for (size_t i = 0; i < responses.size(); ++i)
    PopulateWebResponseFromResponse(responses[i], &(web_responses[i]));
  return web_responses;
}

}  // namespace content
