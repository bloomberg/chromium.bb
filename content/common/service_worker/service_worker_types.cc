// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"

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

ServiceWorkerFetchRequest::~ServiceWorkerFetchRequest() {}

ServiceWorkerResponse::ServiceWorkerResponse()
    : status_code(0),
      response_type(blink::WebServiceWorkerResponseTypeOpaque),
      blob_size(0),
      error(blink::WebServiceWorkerResponseErrorUnknown) {
}

ServiceWorkerResponse::ServiceWorkerResponse(
    const GURL& url,
    int status_code,
    const std::string& status_text,
    blink::WebServiceWorkerResponseType response_type,
    const ServiceWorkerHeaderMap& headers,
    const std::string& blob_uuid,
    uint64_t blob_size,
    const GURL& stream_url,
    blink::WebServiceWorkerResponseError error)
    : url(url),
      status_code(status_code),
      status_text(status_text),
      response_type(response_type),
      headers(headers),
      blob_uuid(blob_uuid),
      blob_size(blob_size),
      stream_url(stream_url),
      error(error) {}

ServiceWorkerResponse::~ServiceWorkerResponse() {}

ServiceWorkerObjectInfo::ServiceWorkerObjectInfo()
    : handle_id(kInvalidServiceWorkerHandleId),
      state(blink::WebServiceWorkerStateUnknown),
      version_id(kInvalidServiceWorkerVersionId) {}

ServiceWorkerRegistrationObjectInfo::ServiceWorkerRegistrationObjectInfo()
    : handle_id(kInvalidServiceWorkerRegistrationHandleId),
      registration_id(kInvalidServiceWorkerRegistrationId) {
}

ServiceWorkerClientQueryOptions::ServiceWorkerClientQueryOptions()
    : client_type(blink::WebServiceWorkerClientTypeWindow),
      include_uncontrolled(false) {
}

}  // namespace content
