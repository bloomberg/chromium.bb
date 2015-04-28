// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_listener.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/common/cache_storage/cache_storage_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheError.h"

namespace content {

namespace {

const uint32 kFilteredMessageClasses[] = {CacheStorageMsgStart};

blink::WebServiceWorkerCacheError ToWebServiceWorkerCacheError(
    CacheStorage::CacheStorageError err) {
  switch (err) {
    case CacheStorage::CACHE_STORAGE_ERROR_NO_ERROR:
      NOTREACHED();
      return blink::WebServiceWorkerCacheErrorNotImplemented;
    case CacheStorage::CACHE_STORAGE_ERROR_NOT_IMPLEMENTED:
      return blink::WebServiceWorkerCacheErrorNotImplemented;
    case CacheStorage::CACHE_STORAGE_ERROR_NOT_FOUND:
      return blink::WebServiceWorkerCacheErrorNotFound;
    case CacheStorage::CACHE_STORAGE_ERROR_EXISTS:
      return blink::WebServiceWorkerCacheErrorExists;
    case CacheStorage::CACHE_STORAGE_ERROR_STORAGE:
      // TODO(jkarlin): Change this to CACHE_STORAGE_ERROR_STORAGE once that's
      // added.
      return blink::WebServiceWorkerCacheErrorNotFound;
    case CacheStorage::CACHE_STORAGE_ERROR_CLOSING:
      // TODO(jkarlin): Update this to CACHE_STORAGE_ERROR_CLOSING once that's
      // added.
      return blink::WebServiceWorkerCacheErrorNotFound;
  }
  NOTREACHED();
  return blink::WebServiceWorkerCacheErrorNotImplemented;
}

// TODO(jkarlin): CacheStorageCache and CacheStorage should share
// an error enum type.
blink::WebServiceWorkerCacheError CacheErrorToWebServiceWorkerCacheError(
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

CacheStorageDispatcherHost::CacheStorageDispatcherHost()
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)) {
}

CacheStorageDispatcherHost::~CacheStorageDispatcherHost() {
}

void CacheStorageDispatcherHost::Init(CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CacheStorageDispatcherHost::CreateCacheListener, this,
                 make_scoped_refptr(context)));
}

void CacheStorageDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool CacheStorageDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CacheStorageDispatcherHost, message)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageHas, OnCacheStorageHas)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageOpen, OnCacheStorageOpen)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageDelete,
                      OnCacheStorageDelete)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageKeys, OnCacheStorageKeys)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageMatch,
                      OnCacheStorageMatch)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return handled;

  DCHECK(cache_listener_);
  cache_listener_->OnMessageReceived(message);
  if (!handled)
    BadMessageReceived();
  return handled;
}

void CacheStorageDispatcherHost::CreateCacheListener(
    CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  cache_listener_.reset(new CacheStorageListener(this, context));
  context_ = context;
}

void CacheStorageDispatcherHost::OnCacheStorageHas(
    int thread_id,
    int request_id,
    const GURL& origin,
    const base::string16& cache_name) {
  TRACE_EVENT0("CacheStorage", "CacheStorageDispatcherHost::OnCacheStorageHas");
  context_->cache_manager()->HasCache(
      origin, base::UTF16ToUTF8(cache_name),
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageHasCallback, this,
                 thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageOpen(
    int thread_id,
    int request_id,
    const GURL& origin,
    const base::string16& cache_name) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageOpen");
  context_->cache_manager()->OpenCache(
      origin, base::UTF16ToUTF8(cache_name),
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageOpenCallback, this,
                 thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageDelete(
    int thread_id,
    int request_id,
    const GURL& origin,
    const base::string16& cache_name) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageDelete");
  context_->cache_manager()->DeleteCache(
      origin, base::UTF16ToUTF8(cache_name),
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageDeleteCallback,
                 this, thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageKeys(int thread_id,
                                                    int request_id,
                                                    const GURL& origin) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageKeys");
  context_->cache_manager()->EnumerateCaches(
      origin,
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageKeysCallback, this,
                 thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageMatch(
    int thread_id,
    int request_id,
    const GURL& origin,
    const ServiceWorkerFetchRequest& request,
    const CacheStorageCacheQueryParams& match_params) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageMatch");

  scoped_ptr<ServiceWorkerFetchRequest> scoped_request(
      new ServiceWorkerFetchRequest(request.url, request.method,
                                    request.headers, request.referrer,
                                    request.is_reload));

  if (match_params.cache_name.empty()) {
    context_->cache_manager()->MatchAllCaches(
        origin, scoped_request.Pass(),
        base::Bind(&CacheStorageDispatcherHost::OnCacheStorageMatchCallback,
                   this, thread_id, request_id));
    return;
  }
  context_->cache_manager()->MatchCache(
      origin, base::UTF16ToUTF8(match_params.cache_name), scoped_request.Pass(),
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageMatchCallback, this,
                 thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageHasCallback(
    int thread_id,
    int request_id,
    bool has_cache,
    CacheStorage::CacheStorageError error) {
  if (error != CacheStorage::CACHE_STORAGE_ERROR_NO_ERROR) {
    Send(new CacheStorageMsg_CacheStorageHasError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }
  if (!has_cache) {
    Send(new CacheStorageMsg_CacheStorageHasError(
        thread_id, request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }
  Send(new CacheStorageMsg_CacheStorageHasSuccess(thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageOpenCallback(
    int thread_id,
    int request_id,
    const scoped_refptr<CacheStorageCache>& cache,
    CacheStorage::CacheStorageError error) {
  if (error != CacheStorage::CACHE_STORAGE_ERROR_NO_ERROR) {
    Send(new CacheStorageMsg_CacheStorageOpenError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }
  CacheStorageListener::CacheID cache_id =
      cache_listener_->StoreCacheReference(cache);
  Send(new CacheStorageMsg_CacheStorageOpenSuccess(thread_id, request_id,
                                                   cache_id));
}

void CacheStorageDispatcherHost::OnCacheStorageDeleteCallback(
    int thread_id,
    int request_id,
    bool deleted,
    CacheStorage::CacheStorageError error) {
  if (!deleted || error != CacheStorage::CACHE_STORAGE_ERROR_NO_ERROR) {
    Send(new CacheStorageMsg_CacheStorageDeleteError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }
  Send(new CacheStorageMsg_CacheStorageDeleteSuccess(thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageKeysCallback(
    int thread_id,
    int request_id,
    const std::vector<std::string>& strings,
    CacheStorage::CacheStorageError error) {
  if (error != CacheStorage::CACHE_STORAGE_ERROR_NO_ERROR) {
    Send(new CacheStorageMsg_CacheStorageKeysError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }

  std::vector<base::string16> string16s;
  for (size_t i = 0, max = strings.size(); i < max; ++i) {
    string16s.push_back(base::UTF8ToUTF16(strings[i]));
  }
  Send(new CacheStorageMsg_CacheStorageKeysSuccess(thread_id, request_id,
                                                   string16s));
}

void CacheStorageDispatcherHost::OnCacheStorageMatchCallback(
    int thread_id,
    int request_id,
    CacheStorageCache::ErrorType error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (error != CacheStorageCache::ERROR_TYPE_OK) {
    Send(new CacheStorageMsg_CacheStorageMatchError(
        thread_id, request_id, CacheErrorToWebServiceWorkerCacheError(error)));
    return;
  }

  if (blob_data_handle)
    cache_listener_->StoreBlobDataHandle(blob_data_handle.Pass());

  Send(new CacheStorageMsg_CacheStorageMatchSuccess(thread_id, request_id,
                                                    *response));
}

}  // namespace content
