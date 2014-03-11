// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest() {}

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest(
    const GURL& url,
    const std::string& method,
    const std::map<std::string, std::string>& headers)
    : url(url),
      method(method),
      headers(headers) {
}

ServiceWorkerFetchRequest::~ServiceWorkerFetchRequest() {}

ServiceWorkerResponse::ServiceWorkerResponse() : status_code(0) {}

ServiceWorkerResponse::ServiceWorkerResponse(
    int status_code,
    const std::string& status_text,
    const std::string& method,
    const std::map<std::string, std::string>& headers)
    : status_code(status_code),
      status_text(status_text),
      method(method),
      headers(headers) {}

ServiceWorkerResponse::~ServiceWorkerResponse() {}

}  // namespace content
