// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_

#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRegistration.h"

namespace content {

struct ServiceWorkerObjectInfo;

class WebServiceWorkerRegistrationImpl
    : NON_EXPORTED_BASE(public blink::WebServiceWorkerRegistration) {
 public:
  explicit WebServiceWorkerRegistrationImpl(
      const ServiceWorkerObjectInfo& info);
  virtual ~WebServiceWorkerRegistrationImpl();

  virtual blink::WebURL scope() const;

 private:
  const GURL scope_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerRegistrationImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
