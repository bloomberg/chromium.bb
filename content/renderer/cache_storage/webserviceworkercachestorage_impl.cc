// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/cache_storage/webserviceworkercachestorage_impl.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "content/child/child_thread_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/cache_storage/cache_storage_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

namespace content {

using base::TimeTicks;
using blink::mojom::CacheStorageError;
using blink::WebServiceWorkerRequest;
using blink::WebString;

namespace {

ServiceWorkerFetchRequest FetchRequestFromWebRequest(
    const WebServiceWorkerRequest& web_request) {
  ServiceWorkerHeaderMap headers;
  GetServiceWorkerHeaderMapFromWebRequest(web_request, &headers);

  return ServiceWorkerFetchRequest(
      web_request.Url(), web_request.Method().Ascii(), headers,
      Referrer(web_request.ReferrerUrl(), web_request.GetReferrerPolicy()),
      web_request.IsReload());
}

void PopulateWebRequestFromFetchRequest(
    const ServiceWorkerFetchRequest& request,
    WebServiceWorkerRequest* web_request) {
  web_request->SetURL(request.url);
  web_request->SetMethod(WebString::FromASCII(request.method));
  for (const auto& header : request.headers) {
    web_request->SetHeader(WebString::FromASCII(header.first),
                           WebString::FromASCII(header.second));
  }
  web_request->SetReferrer(WebString::FromASCII(request.referrer.url.spec()),
                           request.referrer.policy);
  web_request->SetIsReload(request.is_reload);
}

blink::WebVector<WebServiceWorkerRequest> WebRequestsFromRequests(
    const std::vector<ServiceWorkerFetchRequest>& requests) {
  blink::WebVector<WebServiceWorkerRequest> web_requests(requests.size());
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
      WebString::ToNullableString16(web_query_params.cache_name);
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

}  // namespace

// Class to outlive WebCache, we need to keep the ref (and callbacks) around
// even after WebCache is dropped as some consumers like
// InspectorCacheStorageAgent expects to be called back without holding a
// reference to it. CacheRef is ref-counted and the callback holds a reference
// to it to keep it alive until the callback is called.
class WebServiceWorkerCacheStorageImpl::CacheRef
    : public base::RefCounted<CacheRef> {
 public:
  using CacheBatchCallbacks = blink::WebServiceWorkerCache::CacheBatchCallbacks;
  using CacheMatchCallbacks = blink::WebServiceWorkerCache::CacheMatchCallbacks;
  using CacheWithResponsesCallbacks =
      blink::WebServiceWorkerCache::CacheWithResponsesCallbacks;
  using CacheWithRequestsCallbacks =
      blink::WebServiceWorkerCache::CacheWithRequestsCallbacks;

  CacheRef(base::WeakPtr<WebServiceWorkerCacheStorageImpl> dispatcher,
           blink::mojom::CacheStorageCacheAssociatedPtrInfo cache_ptr_info)
      : dispatcher_(std::move(dispatcher)) {
    cache_ptr_.Bind(std::move(cache_ptr_info));
  }

  void DispatchMatch(
      std::unique_ptr<CacheMatchCallbacks> callbacks,
      const WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params) {
    cache_ptr_->Match(FetchRequestFromWebRequest(request),
                      QueryParamsFromWebQueryParams(query_params),
                      base::BindOnce(&CacheRef::CacheMatchCallback, this,
                                     std::move(callbacks), TimeTicks::Now()));
  }

  void DispatchMatchAll(
      std::unique_ptr<CacheWithResponsesCallbacks> callbacks,
      const WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params) {
    cache_ptr_->MatchAll(
        FetchRequestFromWebRequest(request),
        QueryParamsFromWebQueryParams(query_params),
        base::BindOnce(&CacheRef::CacheMatchAllCallback, this,
                       std::move(callbacks), TimeTicks::Now()));
  }

  void DispatchKeys(
      std::unique_ptr<CacheWithRequestsCallbacks> callbacks,
      const WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params) {
    cache_ptr_->Keys(FetchRequestFromWebRequest(request),
                     QueryParamsFromWebQueryParams(query_params),
                     base::BindOnce(&CacheRef::CacheKeysCallback, this,
                                    std::move(callbacks)));
  }

  void DispatchBatch(
      std::unique_ptr<CacheBatchCallbacks> callbacks,
      const blink::WebVector<blink::WebServiceWorkerCache::BatchOperation>&
          batch_operations) {
    std::vector<CacheStorageBatchOperation> operations;
    operations.reserve(batch_operations.size());
    for (size_t i = 0; i < batch_operations.size(); ++i) {
      operations.push_back(
          BatchOperationFromWebBatchOperation(batch_operations[i]));
    }
    cache_ptr_->Batch(operations,
                      base::BindOnce(&CacheRef::BatchCallback, this,
                                     std::move(callbacks), TimeTicks::Now()));
  }

 private:
  friend class base::RefCounted<CacheRef>;

  ~CacheRef() = default;

  void CacheMatchCallback(std::unique_ptr<CacheMatchCallbacks> callbacks,
                          base::TimeTicks start_time,
                          blink::mojom::MatchResultPtr result) {
    if (result->is_status() &&
        result->get_status() != CacheStorageError::kSuccess) {
      callbacks->OnError(result->get_status());
    } else if (!dispatcher_) {
      callbacks->OnError(CacheStorageError::kErrorNotFound);
    } else {
      blink::WebServiceWorkerResponse web_response;
      dispatcher_->PopulateWebResponseFromResponse(result->get_response(),
                                                   &web_response);

      UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Match",
                          TimeTicks::Now() - start_time);
      callbacks->OnSuccess(web_response);
    }
  }

  void CacheMatchAllCallback(
      std::unique_ptr<CacheWithResponsesCallbacks> callbacks,
      base::TimeTicks start_time,
      blink::mojom::MatchAllResultPtr result) {
    if (result->is_status() &&
        result->get_status() != CacheStorageError::kSuccess) {
      callbacks->OnError(result->get_status());
    } else if (!dispatcher_) {
      callbacks->OnError(CacheStorageError::kErrorNotFound);
    } else {
      UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.MatchAll",
                          TimeTicks::Now() - start_time);
      callbacks->OnSuccess(
          dispatcher_->WebResponsesFromResponses(result->get_responses()));
    }
  }

  void CacheKeysCallback(std::unique_ptr<CacheWithRequestsCallbacks> callbacks,
                         blink::mojom::CacheKeysResultPtr result) {
    if (result->is_status() &&
        result->get_status() != CacheStorageError::kSuccess) {
      callbacks->OnError(result->get_status());
    } else {
      callbacks->OnSuccess(WebRequestsFromRequests(result->get_keys()));
    }
  }

  void BatchCallback(std::unique_ptr<CacheBatchCallbacks> callbacks,
                     base::TimeTicks start_time,
                     CacheStorageError error) {
    if (error == CacheStorageError::kSuccess) {
      UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Batch",
                          TimeTicks::Now() - start_time);
      callbacks->OnSuccess();
    } else {
      callbacks->OnError(error);
    }
  }

  blink::mojom::CacheStorageCacheAssociatedPtr cache_ptr_;
  base::WeakPtr<WebServiceWorkerCacheStorageImpl> dispatcher_;
};

// The WebCache object is the Chromium side implementation of the Blink
// WebServiceWorkerCache API. It only delegates the client calls to CacheRef
// object, which have a lifetime longer than the WebCache. WebCache is owned by
// the script-facing caller. CacheRef outlives WebCache to guarantee callback
// responses are invoked, regardless of script-facing keeping WebCache instance
// alive.
class WebServiceWorkerCacheStorageImpl::WebCache
    : public blink::WebServiceWorkerCache {
 public:
  WebCache(scoped_refptr<CacheRef> cache_ref)
      : cache_ref_(std::move(cache_ref)) {}

  ~WebCache() override = default;

  // From blink::WebServiceWorkerCache:
  void DispatchMatch(std::unique_ptr<CacheMatchCallbacks> callbacks,
                     const WebServiceWorkerRequest& request,
                     const QueryParams& query_params) override {
    cache_ref_->DispatchMatch(std::move(callbacks), std::move(request),
                              std::move(query_params));
  }

  void DispatchMatchAll(std::unique_ptr<CacheWithResponsesCallbacks> callbacks,
                        const WebServiceWorkerRequest& request,
                        const QueryParams& query_params) override {
    cache_ref_->DispatchMatchAll(std::move(callbacks), std::move(request),
                                 std::move(query_params));
  }

  void DispatchKeys(std::unique_ptr<CacheWithRequestsCallbacks> callbacks,
                    const WebServiceWorkerRequest& request,
                    const QueryParams& query_params) override {
    cache_ref_->DispatchKeys(std::move(callbacks), std::move(request),
                             std::move(query_params));
  }

  void DispatchBatch(
      std::unique_ptr<CacheBatchCallbacks> callbacks,
      const blink::WebVector<blink::WebServiceWorkerCache::BatchOperation>&
          batch_operations) override {
    cache_ref_->DispatchBatch(std::move(callbacks),
                              std::move(batch_operations));
  }

 private:
  scoped_refptr<CacheRef> cache_ref_;

  DISALLOW_COPY_AND_ASSIGN(WebCache);
};

WebServiceWorkerCacheStorageImpl::WebServiceWorkerCacheStorageImpl(
    service_manager::InterfaceProvider* provider)
    : weak_factory_(this) {
  // Sets up the Mojo InterfacePtr to send IPCs to browser process.
  provider->GetInterface(mojo::MakeRequest(&cache_storage_ptr_));
}

WebServiceWorkerCacheStorageImpl::~WebServiceWorkerCacheStorageImpl() = default;

void WebServiceWorkerCacheStorageImpl::DispatchHas(
    std::unique_ptr<CacheStorageCallbacks> callbacks,
    const WebString& cacheName) {
  GetCacheStorage().Has(
      cacheName.Utf16(),
      base::BindOnce(
          &WebServiceWorkerCacheStorageImpl::OnCacheStorageHasCallback,
          weak_factory_.GetWeakPtr(), std::move(callbacks),
          base::TimeTicks::Now()));
}

void WebServiceWorkerCacheStorageImpl::OnCacheStorageHasCallback(
    std::unique_ptr<CacheStorageCallbacks> callbacks,
    base::TimeTicks start_time,
    CacheStorageError result) {
  if (result == CacheStorageError::kSuccess) {
    UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Has",
                        TimeTicks::Now() - start_time);
    callbacks->OnSuccess();
  } else {
    callbacks->OnError(result);
  }
}

void WebServiceWorkerCacheStorageImpl::DispatchOpen(
    std::unique_ptr<CacheStorageWithCacheCallbacks> callbacks,
    const WebString& cacheName) {
  GetCacheStorage().Open(
      cacheName.Utf16(),
      base::BindOnce(
          &WebServiceWorkerCacheStorageImpl::OnCacheStorageOpenCallback,
          weak_factory_.GetWeakPtr(), std::move(callbacks),
          base::TimeTicks::Now()));
}

void WebServiceWorkerCacheStorageImpl::OnCacheStorageOpenCallback(
    std::unique_ptr<CacheStorageWithCacheCallbacks> callbacks,
    base::TimeTicks start_time,
    blink::mojom::OpenResultPtr result) {
  if (result->is_status() &&
      result->get_status() != CacheStorageError::kSuccess) {
    callbacks->OnError(result->get_status());
  } else {
    std::unique_ptr<WebCache> web_cache =
        std::make_unique<WebCache>(base::MakeRefCounted<CacheRef>(
            weak_factory_.GetWeakPtr(), std::move(result->get_cache())));

    UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Open",
                        TimeTicks::Now() - start_time);
    callbacks->OnSuccess(std::move(web_cache));
  }
}

void WebServiceWorkerCacheStorageImpl::DispatchDelete(
    std::unique_ptr<CacheStorageCallbacks> callbacks,
    const WebString& cacheName) {
  GetCacheStorage().Delete(
      cacheName.Utf16(),
      base::BindOnce(
          &WebServiceWorkerCacheStorageImpl::CacheStorageDeleteCallback,
          weak_factory_.GetWeakPtr(), std::move(callbacks),
          base::TimeTicks::Now()));
}

void WebServiceWorkerCacheStorageImpl::CacheStorageDeleteCallback(
    std::unique_ptr<CacheStorageCallbacks> callbacks,
    base::TimeTicks start_time,
    CacheStorageError result) {
  if (result == CacheStorageError::kSuccess) {
    UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Delete",
                        TimeTicks::Now() - start_time);
    callbacks->OnSuccess();
  } else {
    callbacks->OnError(result);
  }
}

void WebServiceWorkerCacheStorageImpl::DispatchKeys(
    std::unique_ptr<CacheStorageKeysCallbacks> callbacks) {
  GetCacheStorage().Keys(
      base::BindOnce(&WebServiceWorkerCacheStorageImpl::KeysCallback,
                     weak_factory_.GetWeakPtr(), std::move(callbacks),
                     base::TimeTicks::Now()));
}

void WebServiceWorkerCacheStorageImpl::KeysCallback(
    std::unique_ptr<CacheStorageKeysCallbacks> callbacks,
    base::TimeTicks start_time,
    const std::vector<base::string16>& keys) {
  blink::WebVector<WebString> web_keys(keys.size());
  std::transform(
      keys.begin(), keys.end(), web_keys.begin(),
      [](const base::string16& s) { return WebString::FromUTF16(s); });
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Keys",
                      TimeTicks::Now() - start_time);
  callbacks->OnSuccess(web_keys);
}

void WebServiceWorkerCacheStorageImpl::DispatchMatch(
    std::unique_ptr<CacheStorageMatchCallbacks> callbacks,
    const WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  GetCacheStorage().Match(
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params),
      base::BindOnce(
          &WebServiceWorkerCacheStorageImpl::OnCacheStorageMatchCallback,
          weak_factory_.GetWeakPtr(), std::move(callbacks),
          base::TimeTicks::Now()));
}

void WebServiceWorkerCacheStorageImpl::OnCacheStorageMatchCallback(
    std::unique_ptr<CacheStorageMatchCallbacks> callbacks,
    base::TimeTicks start_time,
    blink::mojom::MatchResultPtr result) {
  if (result->is_status() &&
      result->get_status() != CacheStorageError::kSuccess) {
    callbacks->OnError(result->get_status());
  } else {
    UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Match",
                        TimeTicks::Now() - start_time);
    blink::WebServiceWorkerResponse web_response;
    PopulateWebResponseFromResponse(result->get_response(), &web_response);

    callbacks->OnSuccess(web_response);
  }
}

blink::mojom::CacheStorage&
WebServiceWorkerCacheStorageImpl::GetCacheStorage() {
  return *cache_storage_ptr_;
}

void WebServiceWorkerCacheStorageImpl::PopulateWebResponseFromResponse(
    const ServiceWorkerResponse& response,
    blink::WebServiceWorkerResponse* web_response) {
  web_response->SetURLList(response.url_list);
  web_response->SetStatus(response.status_code);
  web_response->SetStatusText(WebString::FromASCII(response.status_text));
  web_response->SetResponseType(response.response_type);
  web_response->SetResponseTime(response.response_time);
  web_response->SetCacheStorageCacheName(
      response.is_in_cache_storage
          ? WebString::FromUTF8(response.cache_storage_cache_name)
          : WebString());
  blink::WebVector<WebString> headers(
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
    DCHECK(response.blob);
    mojo::ScopedMessagePipeHandle blob_pipe;
    if (response.blob)
      blob_pipe = response.blob->Clone().PassInterface().PassHandle();
    web_response->SetBlob(WebString::FromUTF8(response.blob_uuid),
                          response.blob_size, std::move(blob_pipe));
    // Let the host know that it can release its reference to the blob.
    GetCacheStorage().BlobDataHandled(response.blob_uuid);
  }
}

blink::WebVector<blink::WebServiceWorkerResponse>
WebServiceWorkerCacheStorageImpl::WebResponsesFromResponses(
    const std::vector<ServiceWorkerResponse>& responses) {
  blink::WebVector<blink::WebServiceWorkerResponse> web_responses(
      responses.size());
  for (size_t i = 0; i < responses.size(); ++i)
    PopulateWebResponseFromResponse(responses[i], &(web_responses[i]));
  return web_responses;
}

}  // namespace content
