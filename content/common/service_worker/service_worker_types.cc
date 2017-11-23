// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"

#include "content/public/common/service_worker_modes.h"
#include "net/base/load_flags.h"
#include "storage/common/blob_storage/blob_handle.h"

namespace content {

const char kServiceWorkerRegisterErrorPrefix[] =
    "Failed to register a ServiceWorker: ";
const char kServiceWorkerUpdateErrorPrefix[] =
    "Failed to update a ServiceWorker: ";
const char kServiceWorkerUnregisterErrorPrefix[] =
    "Failed to unregister a ServiceWorkerRegistration: ";
const char kServiceWorkerGetRegistrationErrorPrefix[] =
    "Failed to get a ServiceWorkerRegistration: ";
const char kServiceWorkerGetRegistrationsErrorPrefix[] =
    "Failed to get ServiceWorkerRegistration objects: ";
const char kFetchScriptError[] =
    "An unknown error occurred when fetching the script.";

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest() = default;

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest(
    const GURL& url,
    const std::string& method,
    const ServiceWorkerHeaderMap& headers,
    const Referrer& referrer,
    bool is_reload)
    : url(url),
      method(method),
      headers(headers),
      referrer(referrer),
      is_reload(is_reload) {}

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest(
    const ServiceWorkerFetchRequest& other) = default;

ServiceWorkerFetchRequest& ServiceWorkerFetchRequest::operator=(
    const ServiceWorkerFetchRequest& other) = default;

ServiceWorkerFetchRequest::~ServiceWorkerFetchRequest() {}

size_t ServiceWorkerFetchRequest::EstimatedStructSize() {
  size_t size = sizeof(ServiceWorkerFetchRequest);
  size += url.spec().size();
  size += blob_uuid.size();
  size += client_id.size();

  for (const auto& key_and_value : headers) {
    size += key_and_value.first.size();
    size += key_and_value.second.size();
  }

  return size;
}

// static
blink::mojom::FetchCacheMode
ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(int load_flags) {
  if (load_flags & net::LOAD_DISABLE_CACHE)
    return blink::mojom::FetchCacheMode::kNoStore;

  if (load_flags & net::LOAD_VALIDATE_CACHE)
    return blink::mojom::FetchCacheMode::kValidateCache;

  if (load_flags & net::LOAD_BYPASS_CACHE) {
    if (load_flags & net::LOAD_ONLY_FROM_CACHE)
      return blink::mojom::FetchCacheMode::kUnspecifiedForceCacheMiss;
    return blink::mojom::FetchCacheMode::kBypassCache;
  }

  if (load_flags & net::LOAD_SKIP_CACHE_VALIDATION) {
    if (load_flags & net::LOAD_ONLY_FROM_CACHE)
      return blink::mojom::FetchCacheMode::kOnlyIfCached;
    return blink::mojom::FetchCacheMode::kForceCache;
  }

  if (load_flags & net::LOAD_ONLY_FROM_CACHE) {
    DCHECK(!(load_flags & net::LOAD_SKIP_CACHE_VALIDATION));
    DCHECK(!(load_flags & net::LOAD_BYPASS_CACHE));
    return blink::mojom::FetchCacheMode::kUnspecifiedOnlyIfCachedStrict;
  }
  return blink::mojom::FetchCacheMode::kDefault;
}

ServiceWorkerResponse::ServiceWorkerResponse()
    : status_code(0),
      response_type(network::mojom::FetchResponseType::kOpaque),
      blob_size(0),
      error(blink::mojom::ServiceWorkerResponseError::kUnknown) {}

ServiceWorkerResponse::ServiceWorkerResponse(
    std::unique_ptr<std::vector<GURL>> url_list,
    int status_code,
    const std::string& status_text,
    network::mojom::FetchResponseType response_type,
    std::unique_ptr<ServiceWorkerHeaderMap> headers,
    const std::string& blob_uuid,
    uint64_t blob_size,
    scoped_refptr<storage::BlobHandle> blob,
    blink::mojom::ServiceWorkerResponseError error,
    base::Time response_time,
    bool is_in_cache_storage,
    const std::string& cache_storage_cache_name,
    std::unique_ptr<ServiceWorkerHeaderList> cors_exposed_headers)
    : status_code(status_code),
      status_text(status_text),
      response_type(response_type),
      blob_uuid(blob_uuid),
      blob_size(blob_size),
      blob(std::move(blob)),
      error(error),
      response_time(response_time),
      is_in_cache_storage(is_in_cache_storage),
      cache_storage_cache_name(cache_storage_cache_name) {
  this->url_list.swap(*url_list);
  this->headers.swap(*headers);
  this->cors_exposed_header_names.swap(*cors_exposed_headers);
}

ServiceWorkerResponse::ServiceWorkerResponse(
    const ServiceWorkerResponse& other) = default;

ServiceWorkerResponse& ServiceWorkerResponse::operator=(
    const ServiceWorkerResponse& other) = default;

ServiceWorkerResponse::~ServiceWorkerResponse() {}

size_t ServiceWorkerResponse::EstimatedStructSize() {
  size_t size = sizeof(ServiceWorkerResponse);
  for (const auto& url : url_list)
    size += url.spec().size();
  size += blob_uuid.size();
  size += cache_storage_cache_name.size();
  for (const auto& key_and_value : headers) {
    size += key_and_value.first.size();
    size += key_and_value.second.size();
  }
  for (const auto& header : cors_exposed_header_names)
    size += header.size();
  size += side_data_blob_uuid.size();
  return size;
}

ServiceWorkerClientQueryOptions::ServiceWorkerClientQueryOptions()
    : client_type(blink::mojom::ServiceWorkerClientType::kWindow),
      include_uncontrolled(false) {}

ExtendableMessageEventSource::ExtendableMessageEventSource() {}

ExtendableMessageEventSource::ExtendableMessageEventSource(
    const ServiceWorkerClientInfo& client_info)
    : client_info(client_info) {}

ExtendableMessageEventSource::ExtendableMessageEventSource(
    const blink::mojom::ServiceWorkerObjectInfo& service_worker_info)
    : service_worker_info(service_worker_info) {}

}  // namespace content
