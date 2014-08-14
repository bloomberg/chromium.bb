// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_

#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRegistration.h"

namespace blink {
class WebServiceWorker;
class WebServiceWorkerRegistrationProxy;
}

namespace content {

class ServiceWorkerRegistrationHandleReference;
class ThreadSafeSender;
struct ServiceWorkerObjectInfo;

class WebServiceWorkerRegistrationImpl
    : NON_EXPORTED_BASE(public blink::WebServiceWorkerRegistration) {
 public:
  explicit WebServiceWorkerRegistrationImpl(
      scoped_ptr<ServiceWorkerRegistrationHandleReference> handle_ref);
  virtual ~WebServiceWorkerRegistrationImpl();

  void OnUpdateFound();

  virtual void setProxy(blink::WebServiceWorkerRegistrationProxy* proxy);
  virtual blink::WebServiceWorkerRegistrationProxy* proxy();
  virtual void setInstalling(blink::WebServiceWorker* service_worker);
  virtual void setWaiting(blink::WebServiceWorker* service_worker);
  virtual void setActive(blink::WebServiceWorker* service_worker);

  virtual blink::WebURL scope() const;

 private:
  scoped_ptr<ServiceWorkerRegistrationHandleReference> handle_ref_;
  blink::WebServiceWorkerRegistrationProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerRegistrationImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
