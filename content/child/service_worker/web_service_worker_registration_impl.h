// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

namespace blink {
class WebServiceWorker;
class WebServiceWorkerRegistrationProxy;
}

namespace content {

class ServiceWorkerRegistrationHandleReference;
class ThreadSafeSender;
struct ServiceWorkerObjectInfo;

class CONTENT_EXPORT WebServiceWorkerRegistrationImpl
    : NON_EXPORTED_BASE(public blink::WebServiceWorkerRegistration),
      public base::RefCounted<WebServiceWorkerRegistrationImpl> {
 public:
  explicit WebServiceWorkerRegistrationImpl(
      scoped_ptr<ServiceWorkerRegistrationHandleReference> handle_ref);

  void SetInstalling(blink::WebServiceWorker* service_worker);
  void SetWaiting(blink::WebServiceWorker* service_worker);
  void SetActive(blink::WebServiceWorker* service_worker);

  void OnUpdateFound();

  // blink::WebServiceWorkerRegistration overrides.
  void setProxy(blink::WebServiceWorkerRegistrationProxy* proxy) override;
  blink::WebServiceWorkerRegistrationProxy* proxy() override;
  blink::WebURL scope() const override;
  void update(blink::WebServiceWorkerProvider* provider,
              WebServiceWorkerUpdateCallbacks* callbacks) override;
  void unregister(blink::WebServiceWorkerProvider* provider,
                  WebServiceWorkerUnregistrationCallbacks* callbacks) override;

  int64 registration_id() const;

  using WebServiceWorkerRegistrationHandle =
      blink::WebServiceWorkerRegistration::Handle;

  // Creates WebServiceWorkerRegistrationHandle object that owns a reference to
  // this WebServiceWorkerRegistrationImpl object.
  blink::WebPassOwnPtr<WebServiceWorkerRegistrationHandle> CreateHandle();

  // Same with CreateHandle(), but returns a raw pointer to the handle w/ its
  // ownership instead. The caller must manage the ownership. This function must
  // be used only for passing the handle to Blink API that does not support
  // blink::WebPassOwnPtr.
  WebServiceWorkerRegistrationHandle* CreateLeakyHandle();

 private:
  friend class base::RefCounted<WebServiceWorkerRegistrationImpl>;
  ~WebServiceWorkerRegistrationImpl() override;

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
