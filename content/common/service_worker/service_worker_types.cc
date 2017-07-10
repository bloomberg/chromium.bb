// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"

#include "content/public/common/service_worker_modes.h"

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

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest()
    : mode(FETCH_REQUEST_MODE_NO_CORS),
      is_main_resource_load(false),
      request_context_type(REQUEST_CONTEXT_TYPE_UNSPECIFIED),
      frame_type(REQUEST_CONTEXT_FRAME_TYPE_NONE),
      blob_size(0),
      credentials_mode(FETCH_CREDENTIALS_MODE_OMIT),
      redirect_mode(FetchRedirectMode::FOLLOW_MODE),
      is_reload(false),
      fetch_type(ServiceWorkerFetchType::FETCH) {}

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest(
    const GURL& url,
    const std::string& method,
    const ServiceWorkerHeaderMap& headers,
    const Referrer& referrer,
    bool is_reload)
    : mode(FETCH_REQUEST_MODE_NO_CORS),
      is_main_resource_load(false),
      request_context_type(REQUEST_CONTEXT_TYPE_UNSPECIFIED),
      frame_type(REQUEST_CONTEXT_FRAME_TYPE_NONE),
      url(url),
      method(method),
      headers(headers),
      blob_size(0),
      referrer(referrer),
      credentials_mode(FETCH_CREDENTIALS_MODE_OMIT),
      redirect_mode(FetchRedirectMode::FOLLOW_MODE),
      is_reload(is_reload),
      fetch_type(ServiceWorkerFetchType::FETCH) {}

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest(
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

ServiceWorkerResponse::ServiceWorkerResponse()
    : status_code(0),
      response_type(blink::kWebServiceWorkerResponseTypeOpaque),
      blob_size(0),
      error(blink::kWebServiceWorkerResponseErrorUnknown) {}

ServiceWorkerResponse::ServiceWorkerResponse(
    std::unique_ptr<std::vector<GURL>> url_list,
    int status_code,
    const std::string& status_text,
    blink::WebServiceWorkerResponseType response_type,
    std::unique_ptr<ServiceWorkerHeaderMap> headers,
    const std::string& blob_uuid,
    uint64_t blob_size,
    blink::WebServiceWorkerResponseError error,
    base::Time response_time,
    bool is_in_cache_storage,
    const std::string& cache_storage_cache_name,
    std::unique_ptr<ServiceWorkerHeaderList> cors_exposed_headers)
    : status_code(status_code),
      status_text(status_text),
      response_type(response_type),
      blob_uuid(blob_uuid),
      blob_size(blob_size),
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
  return size;
}

ServiceWorkerObjectInfo::ServiceWorkerObjectInfo()
    : handle_id(kInvalidServiceWorkerHandleId),
      state(blink::kWebServiceWorkerStateUnknown),
      version_id(kInvalidServiceWorkerVersionId) {}

bool ServiceWorkerObjectInfo::IsValid() const {
  return handle_id != kInvalidServiceWorkerHandleId &&
         version_id != kInvalidServiceWorkerVersionId;
}

ServiceWorkerRegistrationOptions::ServiceWorkerRegistrationOptions(
    const GURL& scope)
    : scope(scope) {}

ServiceWorkerRegistrationObjectInfo::ServiceWorkerRegistrationObjectInfo()
    : handle_id(kInvalidServiceWorkerRegistrationHandleId),
      registration_id(kInvalidServiceWorkerRegistrationId) {
}

ServiceWorkerClientQueryOptions::ServiceWorkerClientQueryOptions()
    : client_type(blink::kWebServiceWorkerClientTypeWindow),
      include_uncontrolled(false) {}

ExtendableMessageEventSource::ExtendableMessageEventSource() {}

ExtendableMessageEventSource::ExtendableMessageEventSource(
    const ServiceWorkerClientInfo& client_info)
    : client_info(client_info) {}

ExtendableMessageEventSource::ExtendableMessageEventSource(
    const ServiceWorkerObjectInfo& service_worker_info)
    : service_worker_info(service_worker_info) {}

NavigationPreloadState::NavigationPreloadState()
    : enabled(false), header("true") {}

NavigationPreloadState::NavigationPreloadState(bool enabled, std::string header)
    : enabled(enabled), header(header) {}

NavigationPreloadState::NavigationPreloadState(
    const NavigationPreloadState& other) = default;

}  // namespace content
