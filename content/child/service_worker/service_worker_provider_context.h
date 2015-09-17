// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_

#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "content/common/service_worker/service_worker_types.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class ServiceWorkerHandleReference;
class ServiceWorkerRegistrationHandleReference;
struct ServiceWorkerProviderContextDeleter;
class ThreadSafeSender;

// An instance of this class holds information related to Document/Worker.
// Created and destructed on the main thread. Unless otherwise noted, all
// methods are called on the main thread.
class ServiceWorkerProviderContext
    : public base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                        ServiceWorkerProviderContextDeleter> {
 public:
  explicit ServiceWorkerProviderContext(int provider_id);

  // Called from ServiceWorkerDispatcher.
  void OnAssociateRegistration(const ServiceWorkerRegistrationObjectInfo& info,
                               const ServiceWorkerVersionAttributes& attrs);
  void OnDisassociateRegistration();
  void OnSetControllerServiceWorker(const ServiceWorkerObjectInfo& info);

  int provider_id() const { return provider_id_; }

  ServiceWorkerHandleReference* controller();

  // Called on the worker thread.
  bool GetRegistrationInfoAndVersionAttributes(
      ServiceWorkerRegistrationObjectInfo* info,
      ServiceWorkerVersionAttributes* attrs);

 private:
  friend class base::DeleteHelper<ServiceWorkerProviderContext>;
  friend class base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                          ServiceWorkerProviderContextDeleter>;
  friend struct ServiceWorkerProviderContextDeleter;

  ~ServiceWorkerProviderContext();
  void DestructOnMainThread() const;

  const int provider_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Protects (installing, waiting, active) worker and registration references.
  base::Lock lock_;

  // Used on both the main thread and the worker thread.
  scoped_ptr<ServiceWorkerHandleReference> installing_;
  scoped_ptr<ServiceWorkerHandleReference> waiting_;
  scoped_ptr<ServiceWorkerHandleReference> active_;
  scoped_ptr<ServiceWorkerRegistrationHandleReference> registration_;

  // Used only on the main thread.
  scoped_ptr<ServiceWorkerHandleReference> controller_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContext);
};

struct ServiceWorkerProviderContextDeleter {
  static void Destruct(const ServiceWorkerProviderContext* context) {
    context->DestructOnMainThread();
  }
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
