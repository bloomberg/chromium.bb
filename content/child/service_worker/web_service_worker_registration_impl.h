// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_

#include <vector>

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

  void SetInstalling(blink::WebServiceWorker* service_worker);
  void SetWaiting(blink::WebServiceWorker* service_worker);
  void SetActive(blink::WebServiceWorker* service_worker);

  void OnUpdateFound();

  // blink::WebServiceWorkerRegistration overrides.
  virtual void setProxy(blink::WebServiceWorkerRegistrationProxy* proxy);
  virtual blink::WebServiceWorkerRegistrationProxy* proxy();
  virtual blink::WebURL scope() const;

 private:
  enum QueuedTaskType {
    INSTALLING,
    WAITING,
    ACTIVE,
    UPDATE_FOUND,
  };

  struct QueuedTask {
    QueuedTask(QueuedTaskType type,
               blink::WebServiceWorker* worker);
    QueuedTaskType type;
    blink::WebServiceWorker* worker;
  };

  void RunQueuedTasks();
  void ClearQueuedTasks();

  scoped_ptr<ServiceWorkerRegistrationHandleReference> handle_ref_;
  blink::WebServiceWorkerRegistrationProxy* proxy_;

  std::vector<QueuedTask> queued_tasks_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerRegistrationImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
