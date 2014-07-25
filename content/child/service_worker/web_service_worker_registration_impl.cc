// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_registration_impl.h"

#include "content/common/service_worker/service_worker_types.h"

namespace content {

WebServiceWorkerRegistrationImpl::WebServiceWorkerRegistrationImpl(
    const ServiceWorkerObjectInfo& info)
    : scope_(info.scope) {
}

WebServiceWorkerRegistrationImpl::~WebServiceWorkerRegistrationImpl() {
}

blink::WebURL WebServiceWorkerRegistrationImpl::scope() const {
  return scope_;
}

}  // namespace content
