// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_listener.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_cache.h"
#include "content/browser/service_worker/service_worker_cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheError.h"

namespace content {

using blink::WebServiceWorkerCacheError;

namespace {

WebServiceWorkerCacheError ToWebServiceWorkerCacheError(
    ServiceWorkerCacheStorage::CacheStorageError err) {
  switch (err) {
    case ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR:
      NOTREACHED();
      return blink::WebServiceWorkerCacheErrorNotImplemented;
    case ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NOT_IMPLEMENTED:
      return blink::WebServiceWorkerCacheErrorNotImplemented;
    case ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NOT_FOUND:
      return blink::WebServiceWorkerCacheErrorNotFound;
    case ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_EXISTS:
      return blink::WebServiceWorkerCacheErrorExists;
    case ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_STORAGE:
      // TODO(jkarlin): Change this to CACHE_STORAGE_ERROR_STORAGE once that's
      // added.
      return blink::WebServiceWorkerCacheErrorNotFound;
    case ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_CLOSING:
      // TODO(jkarlin): Update this to CACHE_STORAGE_ERROR_CLOSING once that's
      // added.
      return blink::WebServiceWorkerCacheErrorNotFound;
  }
  NOTREACHED();
  return blink::WebServiceWorkerCacheErrorNotImplemented;
}

// TODO(jkarlin): ServiceWorkerCache and ServiceWorkerCacheStorage should share
// an error enum type.
WebServiceWorkerCacheError CacheErrorToWebServiceWorkerCacheError(
    ServiceWorkerCache::ErrorType err) {
  switch (err) {
    case ServiceWorkerCache::ERROR_TYPE_OK:
      NOTREACHED();
      return blink::WebServiceWorkerCacheErrorNotImplemented;
    case ServiceWorkerCache::ERROR_TYPE_EXISTS:
      return blink::WebServiceWorkerCacheErrorExists;
    case ServiceWorkerCache::ERROR_TYPE_STORAGE:
      // TODO(jkarlin): Change this to CACHE_STORAGE_ERROR_STORAGE once that's
      // added.
      return blink::WebServiceWorkerCacheErrorNotFound;
    case ServiceWorkerCache::ERROR_TYPE_NOT_FOUND:
      return blink::WebServiceWorkerCacheErrorNotFound;
  }
  NOTREACHED();
  return blink::WebServiceWorkerCacheErrorNotImplemented;
}

}  // namespace

ServiceWorkerCacheListener::ServiceWorkerCacheListener(
    ServiceWorkerVersion* version,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : version_(version),
      context_(context),
      next_cache_id_(0),
      weak_factory_(this) {
  version_->embedded_worker()->AddListener(this);
}

ServiceWorkerCacheListener::~ServiceWorkerCacheListener() {
  version_->embedded_worker()->RemoveListener(this);
}

bool ServiceWorkerCacheListener::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerCacheListener, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageHas,
                        OnCacheStorageHas)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageOpen,
                        OnCacheStorageOpen)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageDelete,
                        OnCacheStorageDelete)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageKeys,
                        OnCacheStorageKeys)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageMatch,
                        OnCacheStorageMatch)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheMatch,
                        OnCacheMatch)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheMatchAll,
                        OnCacheMatchAll)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheKeys,
                        OnCacheKeys)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheBatch,
                        OnCacheBatch)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheClosed,
                        OnCacheClosed)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_BlobDataHandled, OnBlobDataHandled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ServiceWorkerCacheListener::OnCacheStorageHas(
    int request_id,
    const base::string16& cache_name) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerCacheListener::OnCacheStorageHas");
  context_->cache_manager()->HasCache(
      version_->scope().GetOrigin(),
      base::UTF16ToUTF8(cache_name),
      base::Bind(&ServiceWorkerCacheListener::OnCacheStorageHasCallback,
                 weak_factory_.GetWeakPtr(),
                 request_id));
}

void ServiceWorkerCacheListener::OnCacheStorageOpen(
    int request_id,
    const base::string16& cache_name) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerCacheListener::OnCacheStorageOpen");
  context_->cache_manager()->OpenCache(
      version_->scope().GetOrigin(),
      base::UTF16ToUTF8(cache_name),
      base::Bind(&ServiceWorkerCacheListener::OnCacheStorageOpenCallback,
                 weak_factory_.GetWeakPtr(),
                 request_id));
}

void ServiceWorkerCacheListener::OnCacheStorageDelete(
    int request_id,
    const base::string16& cache_name) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerCacheListener::OnCacheStorageDelete");
  context_->cache_manager()->DeleteCache(
      version_->scope().GetOrigin(),
      base::UTF16ToUTF8(cache_name),
      base::Bind(&ServiceWorkerCacheListener::OnCacheStorageDeleteCallback,
                 weak_factory_.GetWeakPtr(),
                 request_id));
}

void ServiceWorkerCacheListener::OnCacheStorageKeys(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerCacheListener::OnCacheStorageKeys");
  context_->cache_manager()->EnumerateCaches(
      version_->scope().GetOrigin(),
      base::Bind(&ServiceWorkerCacheListener::OnCacheStorageKeysCallback,
                 weak_factory_.GetWeakPtr(),
                 request_id));
}

void ServiceWorkerCacheListener::OnCacheStorageMatch(
    int request_id,
    const ServiceWorkerFetchRequest& request,
    const ServiceWorkerCacheQueryParams& match_params) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerCacheListener::OnCacheStorageMatch");

  scoped_ptr<ServiceWorkerFetchRequest> scoped_request(
      new ServiceWorkerFetchRequest(request.url, request.method,
                                    request.headers, request.referrer,
                                    request.is_reload));

  if (match_params.cache_name.empty()) {
    context_->cache_manager()->MatchAllCaches(
        version_->scope().GetOrigin(), scoped_request.Pass(),
        base::Bind(&ServiceWorkerCacheListener::OnCacheStorageMatchCallback,
                   weak_factory_.GetWeakPtr(), request_id));
    return;
  }
  context_->cache_manager()->MatchCache(
      version_->scope().GetOrigin(), base::UTF16ToUTF8(match_params.cache_name),
      scoped_request.Pass(),
      base::Bind(&ServiceWorkerCacheListener::OnCacheStorageMatchCallback,
                 weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerCacheListener::OnCacheMatch(
    int request_id,
    int cache_id,
    const ServiceWorkerFetchRequest& request,
    const ServiceWorkerCacheQueryParams& match_params) {
  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end()) {
    Send(ServiceWorkerMsg_CacheMatchError(
        request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  scoped_refptr<ServiceWorkerCache> cache = it->second;
  scoped_ptr<ServiceWorkerFetchRequest> scoped_request(
      new ServiceWorkerFetchRequest(request.url,
                                    request.method,
                                    request.headers,
                                    request.referrer,
                                    request.is_reload));
  cache->Match(scoped_request.Pass(),
               base::Bind(&ServiceWorkerCacheListener::OnCacheMatchCallback,
                          weak_factory_.GetWeakPtr(),
                          request_id,
                          cache));
}

void ServiceWorkerCacheListener::OnCacheMatchAll(
    int request_id,
    int cache_id,
    const ServiceWorkerFetchRequest& request,
    const ServiceWorkerCacheQueryParams& match_params) {
  // TODO(gavinp,jkarlin): Implement this method.
  Send(ServiceWorkerMsg_CacheMatchAllError(
      request_id, blink::WebServiceWorkerCacheErrorNotImplemented));
}

void ServiceWorkerCacheListener::OnCacheKeys(
    int request_id,
    int cache_id,
    const ServiceWorkerFetchRequest& request,
    const ServiceWorkerCacheQueryParams& match_params) {
  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end()) {
    Send(ServiceWorkerMsg_CacheKeysError(
        request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  scoped_refptr<ServiceWorkerCache> cache = it->second;

  cache->Keys(base::Bind(&ServiceWorkerCacheListener::OnCacheKeysCallback,
                         weak_factory_.GetWeakPtr(),
                         request_id,
                         cache));
}

void ServiceWorkerCacheListener::OnCacheBatch(
    int request_id,
    int cache_id,
    const std::vector<ServiceWorkerBatchOperation>& operations) {
  if (operations.size() != 1u) {
    Send(ServiceWorkerMsg_CacheBatchError(
        request_id, blink::WebServiceWorkerCacheErrorNotImplemented));
    return;
  }

  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end()) {
    Send(ServiceWorkerMsg_CacheBatchError(
        request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  const ServiceWorkerBatchOperation& operation = operations[0];

  scoped_refptr<ServiceWorkerCache> cache = it->second;
  scoped_ptr<ServiceWorkerFetchRequest> scoped_request(
      new ServiceWorkerFetchRequest(operation.request.url,
                                    operation.request.method,
                                    operation.request.headers,
                                    operation.request.referrer,
                                    operation.request.is_reload));

  if (operation.operation_type == SERVICE_WORKER_CACHE_OPERATION_TYPE_DELETE) {
    cache->Delete(scoped_request.Pass(),
                  base::Bind(&ServiceWorkerCacheListener::OnCacheDeleteCallback,
                             weak_factory_.GetWeakPtr(),
                             request_id,
                             cache));
    return;
  }

  if (operation.operation_type == SERVICE_WORKER_CACHE_OPERATION_TYPE_PUT) {
    // We don't support streaming for cache.
    DCHECK(operation.response.stream_url.is_empty());
    scoped_ptr<ServiceWorkerResponse> scoped_response(
        new ServiceWorkerResponse(operation.response.url,
                                  operation.response.status_code,
                                  operation.response.status_text,
                                  operation.response.response_type,
                                  operation.response.headers,
                                  operation.response.blob_uuid,
                                  operation.response.blob_size,
                                  operation.response.stream_url));
    cache->Put(scoped_request.Pass(),
               scoped_response.Pass(),
               base::Bind(&ServiceWorkerCacheListener::OnCachePutCallback,
                          weak_factory_.GetWeakPtr(),
                          request_id,
                          cache));

    return;
  }

  Send(ServiceWorkerMsg_CacheBatchError(
      request_id, blink::WebServiceWorkerCacheErrorNotImplemented));
}

void ServiceWorkerCacheListener::OnCacheClosed(int cache_id) {
  DropCacheReference(cache_id);
}

void ServiceWorkerCacheListener::OnBlobDataHandled(const std::string& uuid) {
  DropBlobDataHandle(uuid);
}

void ServiceWorkerCacheListener::Send(const IPC::Message& message) {
  version_->embedded_worker()->SendMessage(message);
}

void ServiceWorkerCacheListener::OnCacheStorageHasCallback(
    int request_id,
    bool has_cache,
    ServiceWorkerCacheStorage::CacheStorageError error) {
  if (error != ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR) {
    Send(ServiceWorkerMsg_CacheStorageHasError(
        request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }
  if (!has_cache) {
    Send(ServiceWorkerMsg_CacheStorageHasError(
        request_id,
        blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }
  Send(ServiceWorkerMsg_CacheStorageHasSuccess(request_id));
}

void ServiceWorkerCacheListener::OnCacheStorageOpenCallback(
    int request_id,
    const scoped_refptr<ServiceWorkerCache>& cache,
    ServiceWorkerCacheStorage::CacheStorageError error) {
  if (error != ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR) {
    Send(ServiceWorkerMsg_CacheStorageOpenError(
        request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }
  CacheID cache_id = StoreCacheReference(cache);
  Send(ServiceWorkerMsg_CacheStorageOpenSuccess(request_id, cache_id));
}

void ServiceWorkerCacheListener::OnCacheStorageDeleteCallback(
    int request_id,
    bool deleted,
    ServiceWorkerCacheStorage::CacheStorageError error) {
  if (!deleted ||
      error != ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR) {
    Send(ServiceWorkerMsg_CacheStorageDeleteError(
        request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }
  Send(ServiceWorkerMsg_CacheStorageDeleteSuccess(request_id));
}

void ServiceWorkerCacheListener::OnCacheStorageKeysCallback(
    int request_id,
    const std::vector<std::string>& strings,
    ServiceWorkerCacheStorage::CacheStorageError error) {
  if (error != ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR) {
    Send(ServiceWorkerMsg_CacheStorageKeysError(
        request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }

  std::vector<base::string16> string16s;
  for (size_t i = 0, max = strings.size(); i < max; ++i) {
    string16s.push_back(base::UTF8ToUTF16(strings[i]));
  }
  Send(ServiceWorkerMsg_CacheStorageKeysSuccess(request_id, string16s));
}

void ServiceWorkerCacheListener::OnCacheStorageMatchCallback(
    int request_id,
    ServiceWorkerCache::ErrorType error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (error != ServiceWorkerCache::ERROR_TYPE_OK) {
    Send(ServiceWorkerMsg_CacheStorageMatchError(
        request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  if (blob_data_handle)
    StoreBlobDataHandle(blob_data_handle.Pass());

  Send(ServiceWorkerMsg_CacheStorageMatchSuccess(request_id, *response));
}

void ServiceWorkerCacheListener::OnCacheMatchCallback(
    int request_id,
    const scoped_refptr<ServiceWorkerCache>& cache,
    ServiceWorkerCache::ErrorType error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (error != ServiceWorkerCache::ERROR_TYPE_OK) {
    Send(ServiceWorkerMsg_CacheMatchError(
        request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  if (blob_data_handle)
    StoreBlobDataHandle(blob_data_handle.Pass());

  Send(ServiceWorkerMsg_CacheMatchSuccess(request_id, *response));
}

void ServiceWorkerCacheListener::OnCacheKeysCallback(
    int request_id,
    const scoped_refptr<ServiceWorkerCache>& cache,
    ServiceWorkerCache::ErrorType error,
    scoped_ptr<ServiceWorkerCache::Requests> requests) {
  if (error != ServiceWorkerCache::ERROR_TYPE_OK) {
    Send(ServiceWorkerMsg_CacheKeysError(
        request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  ServiceWorkerCache::Requests out;

  for (ServiceWorkerCache::Requests::const_iterator it = requests->begin();
       it != requests->end();
       ++it) {
    ServiceWorkerFetchRequest request(
        it->url, it->method, it->headers, it->referrer, it->is_reload);
    out.push_back(request);
  }

  Send(ServiceWorkerMsg_CacheKeysSuccess(request_id, out));
}

void ServiceWorkerCacheListener::OnCacheDeleteCallback(
    int request_id,
    const scoped_refptr<ServiceWorkerCache>& cache,
    ServiceWorkerCache::ErrorType error) {
  if (error != ServiceWorkerCache::ERROR_TYPE_OK) {
    Send(ServiceWorkerMsg_CacheBatchError(
        request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  Send(ServiceWorkerMsg_CacheBatchSuccess(
      request_id, std::vector<ServiceWorkerResponse>()));
}

void ServiceWorkerCacheListener::OnCachePutCallback(
    int request_id,
    const scoped_refptr<ServiceWorkerCache>& cache,
    ServiceWorkerCache::ErrorType error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (error != ServiceWorkerCache::ERROR_TYPE_OK) {
    Send(ServiceWorkerMsg_CacheBatchError(
        request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  if (blob_data_handle)
    StoreBlobDataHandle(blob_data_handle.Pass());

  std::vector<ServiceWorkerResponse> responses;
  responses.push_back(*response);
  Send(ServiceWorkerMsg_CacheBatchSuccess(request_id, responses));
}

ServiceWorkerCacheListener::CacheID
ServiceWorkerCacheListener::StoreCacheReference(
    const scoped_refptr<ServiceWorkerCache>& cache) {
  int cache_id = next_cache_id_++;
  id_to_cache_map_[cache_id] = cache;
  return cache_id;
}

void ServiceWorkerCacheListener::DropCacheReference(CacheID cache_id) {
  id_to_cache_map_.erase(cache_id);
}

void ServiceWorkerCacheListener::StoreBlobDataHandle(
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  DCHECK(blob_data_handle);
  std::pair<UUIDToBlobDataHandleList::iterator, bool> rv =
      blob_handle_store_.insert(std::make_pair(
          blob_data_handle->uuid(), std::list<storage::BlobDataHandle>()));
  rv.first->second.push_front(storage::BlobDataHandle(*blob_data_handle));
}

void ServiceWorkerCacheListener::DropBlobDataHandle(std::string uuid) {
  UUIDToBlobDataHandleList::iterator it = blob_handle_store_.find(uuid);
  if (it == blob_handle_store_.end())
    return;
  DCHECK(!it->second.empty());
  it->second.pop_front();
  if (it->second.empty())
    blob_handle_store_.erase(it);
}

}  // namespace content
