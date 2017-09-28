// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace blink {
class WebServiceWorkerRegistrationProxy;
}

namespace content {

class WebServiceWorkerImpl;

// Each instance corresponds to one ServiceWorkerRegistration object in JS
// context, and is held by ServiceWorkerRegistration object in Blink's C++ layer
// via WebServiceWorkerRegistration::Handle.
//
// Each instance holds one mojo connection of interface
// blink::mojom::ServiceWorkerRegistrationObjectHost inside |info_|, so that
// corresponding ServiceWorkerRegistrationHandle doesn't go away in the browser
// process while the ServiceWorkerRegistration object is alive.
class CONTENT_EXPORT WebServiceWorkerRegistrationImpl
    : public blink::WebServiceWorkerRegistration,
      public base::RefCounted<WebServiceWorkerRegistrationImpl> {
 public:
  explicit WebServiceWorkerRegistrationImpl(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info);

  void SetInstalling(const scoped_refptr<WebServiceWorkerImpl>& service_worker);
  void SetWaiting(const scoped_refptr<WebServiceWorkerImpl>& service_worker);
  void SetActive(const scoped_refptr<WebServiceWorkerImpl>& service_worker);

  void OnUpdateFound();

  // blink::WebServiceWorkerRegistration overrides.
  void SetProxy(blink::WebServiceWorkerRegistrationProxy* proxy) override;
  blink::WebServiceWorkerRegistrationProxy* Proxy() override;
  blink::WebURL Scope() const override;
  void Update(
      blink::WebServiceWorkerProvider* provider,
      std::unique_ptr<WebServiceWorkerUpdateCallbacks> callbacks) override;
  void Unregister(blink::WebServiceWorkerProvider* provider,
                  std::unique_ptr<WebServiceWorkerUnregistrationCallbacks>
                      callbacks) override;
  void EnableNavigationPreload(
      bool enable,
      blink::WebServiceWorkerProvider* provider,
      std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks) override;
  void GetNavigationPreloadState(
      blink::WebServiceWorkerProvider* provider,
      std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks)
      override;
  void SetNavigationPreloadHeader(
      const blink::WebString& value,
      blink::WebServiceWorkerProvider* provider,
      std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks> callbacks)
      override;
  int64_t RegistrationId() const override;

  using WebServiceWorkerRegistrationHandle =
      blink::WebServiceWorkerRegistration::Handle;

  // Creates WebServiceWorkerRegistrationHandle object that owns a reference to
  // the given WebServiceWorkerRegistrationImpl object.
  static std::unique_ptr<WebServiceWorkerRegistrationHandle> CreateHandle(
      const scoped_refptr<WebServiceWorkerRegistrationImpl>& registration);

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
               const scoped_refptr<WebServiceWorkerImpl>& worker);
    QueuedTask(const QueuedTask& other);
    ~QueuedTask();
    QueuedTaskType type;
    scoped_refptr<WebServiceWorkerImpl> worker;
  };

  void RunQueuedTasks();

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info_;
  blink::WebServiceWorkerRegistrationProxy* proxy_;

  std::vector<QueuedTask> queued_tasks_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerRegistrationImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
