// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_listener.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/common/cache_storage/cache_storage_messages.h"
#include "content/common/cache_storage/cache_storage_types.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheError.h"

namespace content {

using blink::WebServiceWorkerCacheError;

namespace {

// TODO(nhiroki): The same function exists in cache_storage_dispatcher_host.cc.
// This should be removed when this listenr class is merged into the dispatcher.
// (http://crbug.com/474233)
WebServiceWorkerCacheError CacheErrorToWebServiceWorkerCacheError(
    CacheStorageCache::ErrorType err) {
  switch (err) {
    case CacheStorageCache::ERROR_TYPE_OK:
      NOTREACHED();
      return blink::WebServiceWorkerCacheErrorNotImplemented;
    case CacheStorageCache::ERROR_TYPE_EXISTS:
      return blink::WebServiceWorkerCacheErrorExists;
    case CacheStorageCache::ERROR_TYPE_STORAGE:
      // TODO(jkarlin): Change this to CACHE_STORAGE_ERROR_STORAGE once that's
      // added.
      return blink::WebServiceWorkerCacheErrorNotFound;
    case CacheStorageCache::ERROR_TYPE_NOT_FOUND:
      return blink::WebServiceWorkerCacheErrorNotFound;
  }
  NOTREACHED();
  return blink::WebServiceWorkerCacheErrorNotImplemented;
}

}  // namespace

CacheStorageListener::CacheStorageListener(
    CacheStorageDispatcherHost* dispatcher,
    CacheStorageContextImpl* context)
    : dispatcher_(dispatcher), context_(context), weak_factory_(this) {
}

CacheStorageListener::~CacheStorageListener() {
}

bool CacheStorageListener::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CacheStorageListener, message)
    IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheMatch, OnCacheMatch)
    IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheMatchAll, OnCacheMatchAll)
    IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheKeys, OnCacheKeys)
    IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheBatch, OnCacheBatch)
    IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheClosed, OnCacheClosed)
    IPC_MESSAGE_HANDLER(CacheStorageHostMsg_BlobDataHandled,
                        OnBlobDataHandled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CacheStorageListener::OnCacheMatch(
    int thread_id,
    int request_id,
    int cache_id,
    const ServiceWorkerFetchRequest& request,
    const CacheStorageCacheQueryParams& match_params) {
  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end()) {
    Send(new CacheStorageMsg_CacheMatchError(
        thread_id, request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  scoped_refptr<CacheStorageCache> cache = it->second;
  scoped_ptr<ServiceWorkerFetchRequest> scoped_request(
      new ServiceWorkerFetchRequest(request.url, request.method,
                                    request.headers, request.referrer,
                                    request.is_reload));
  cache->Match(
      scoped_request.Pass(),
      base::Bind(&CacheStorageListener::OnCacheMatchCallback,
                 weak_factory_.GetWeakPtr(), thread_id, request_id, cache));
}

void CacheStorageListener::OnCacheMatchAll(
    int thread_id,
    int request_id,
    int cache_id,
    const ServiceWorkerFetchRequest& request,
    const CacheStorageCacheQueryParams& match_params) {
  // TODO(gavinp,jkarlin): Implement this method.
  Send(new CacheStorageMsg_CacheMatchAllError(
      thread_id, request_id, blink::WebServiceWorkerCacheErrorNotImplemented));
}

void CacheStorageListener::OnCacheKeys(
    int thread_id,
    int request_id,
    int cache_id,
    const ServiceWorkerFetchRequest& request,
    const CacheStorageCacheQueryParams& match_params) {
  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end()) {
    Send(new CacheStorageMsg_CacheKeysError(
        thread_id, request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  scoped_refptr<CacheStorageCache> cache = it->second;

  cache->Keys(base::Bind(&CacheStorageListener::OnCacheKeysCallback,
                         weak_factory_.GetWeakPtr(), thread_id, request_id,
                         cache));
}

void CacheStorageListener::OnCacheBatch(
    int thread_id,
    int request_id,
    int cache_id,
    const std::vector<CacheStorageBatchOperation>& operations) {
  if (operations.size() != 1u) {
    Send(new CacheStorageMsg_CacheBatchError(
        thread_id, request_id,
        blink::WebServiceWorkerCacheErrorNotImplemented));
    return;
  }

  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end()) {
    Send(new CacheStorageMsg_CacheBatchError(
        thread_id, request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  const CacheStorageBatchOperation& operation = operations[0];

  scoped_refptr<CacheStorageCache> cache = it->second;
  scoped_ptr<ServiceWorkerFetchRequest> scoped_request(
      new ServiceWorkerFetchRequest(
          operation.request.url, operation.request.method,
          operation.request.headers, operation.request.referrer,
          operation.request.is_reload));

  if (operation.operation_type == CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE) {
    cache->Delete(
        scoped_request.Pass(),
        base::Bind(&CacheStorageListener::OnCacheDeleteCallback,
                   weak_factory_.GetWeakPtr(), thread_id, request_id, cache));
    return;
  }

  if (operation.operation_type == CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT) {
    // We don't support streaming for cache.
    DCHECK(operation.response.stream_url.is_empty());
    scoped_ptr<ServiceWorkerResponse> scoped_response(new ServiceWorkerResponse(
        operation.response.url, operation.response.status_code,
        operation.response.status_text, operation.response.response_type,
        operation.response.headers, operation.response.blob_uuid,
        operation.response.blob_size, operation.response.stream_url));
    cache->Put(
        scoped_request.Pass(), scoped_response.Pass(),
        base::Bind(&CacheStorageListener::OnCachePutCallback,
                   weak_factory_.GetWeakPtr(), thread_id, request_id, cache));

    return;
  }

  Send(new CacheStorageMsg_CacheBatchError(
      thread_id, request_id, blink::WebServiceWorkerCacheErrorNotImplemented));
}

void CacheStorageListener::OnCacheClosed(int cache_id) {
  DropCacheReference(cache_id);
}

void CacheStorageListener::OnBlobDataHandled(const std::string& uuid) {
  DropBlobDataHandle(uuid);
}

void CacheStorageListener::Send(IPC::Message* message) {
  dispatcher_->Send(message);
}

void CacheStorageListener::OnCacheMatchCallback(
    int thread_id,
    int request_id,
    const scoped_refptr<CacheStorageCache>& cache,
    CacheStorageCache::ErrorType error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (error != CacheStorageCache::ERROR_TYPE_OK) {
    Send(new CacheStorageMsg_CacheMatchError(
        thread_id, request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  if (blob_data_handle)
    StoreBlobDataHandle(blob_data_handle.Pass());

  Send(new CacheStorageMsg_CacheMatchSuccess(thread_id, request_id, *response));
}

void CacheStorageListener::OnCacheKeysCallback(
    int thread_id,
    int request_id,
    const scoped_refptr<CacheStorageCache>& cache,
    CacheStorageCache::ErrorType error,
    scoped_ptr<CacheStorageCache::Requests> requests) {
  if (error != CacheStorageCache::ERROR_TYPE_OK) {
    Send(new CacheStorageMsg_CacheKeysError(
        thread_id, request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  CacheStorageCache::Requests out;

  for (CacheStorageCache::Requests::const_iterator it = requests->begin();
       it != requests->end(); ++it) {
    ServiceWorkerFetchRequest request(it->url, it->method, it->headers,
                                      it->referrer, it->is_reload);
    out.push_back(request);
  }

  Send(new CacheStorageMsg_CacheKeysSuccess(thread_id, request_id, out));
}

void CacheStorageListener::OnCacheDeleteCallback(
    int thread_id,
    int request_id,
    const scoped_refptr<CacheStorageCache>& cache,
    CacheStorageCache::ErrorType error) {
  if (error != CacheStorageCache::ERROR_TYPE_OK) {
    Send(new CacheStorageMsg_CacheBatchError(
        thread_id, request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  Send(new CacheStorageMsg_CacheBatchSuccess(
      thread_id, request_id, std::vector<ServiceWorkerResponse>()));
}

void CacheStorageListener::OnCachePutCallback(
    int thread_id,
    int request_id,
    const scoped_refptr<CacheStorageCache>& cache,
    CacheStorageCache::ErrorType error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (error != CacheStorageCache::ERROR_TYPE_OK) {
    Send(new CacheStorageMsg_CacheBatchError(
        thread_id, request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  if (blob_data_handle)
    StoreBlobDataHandle(blob_data_handle.Pass());

  std::vector<ServiceWorkerResponse> responses;
  responses.push_back(*response);
  Send(new CacheStorageMsg_CacheBatchSuccess(thread_id, request_id, responses));
}

CacheStorageListener::CacheID CacheStorageListener::StoreCacheReference(
    const scoped_refptr<CacheStorageCache>& cache) {
  int cache_id = next_cache_id_++;
  id_to_cache_map_[cache_id] = cache;
  return cache_id;
}

void CacheStorageListener::DropCacheReference(CacheID cache_id) {
  id_to_cache_map_.erase(cache_id);
}

void CacheStorageListener::StoreBlobDataHandle(
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  DCHECK(blob_data_handle);
  std::pair<UUIDToBlobDataHandleList::iterator, bool> rv =
      blob_handle_store_.insert(std::make_pair(
          blob_data_handle->uuid(), std::list<storage::BlobDataHandle>()));
  rv.first->second.push_front(storage::BlobDataHandle(*blob_data_handle));
}

void CacheStorageListener::DropBlobDataHandle(std::string uuid) {
  UUIDToBlobDataHandleList::iterator it = blob_handle_store_.find(uuid);
  if (it == blob_handle_store_.end())
    return;
  DCHECK(!it->second.empty());
  it->second.pop_front();
  if (it->second.empty())
    blob_handle_store_.erase(it);
}

}  // namespace content
