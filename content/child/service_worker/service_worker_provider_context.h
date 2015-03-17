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
class MessageLoopProxy;
}

namespace IPC {
class Message;
}

namespace content {

class ServiceWorkerHandleReference;
class ServiceWorkerMessageSender;
class ServiceWorkerRegistrationHandleReference;
struct ServiceWorkerProviderContextDeleter;

// An instance of this class holds information related to Document/Worker.
// Created and destructed on the main thread. Some functions can be called
// on the worker thread (eg. GetVersionAttributes).
class ServiceWorkerProviderContext
    : public base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                        ServiceWorkerProviderContextDeleter> {
 public:
  explicit ServiceWorkerProviderContext(int provider_id);

  // Called from ServiceWorkerDispatcher.
  void OnAssociateRegistration(const ServiceWorkerRegistrationObjectInfo& info,
                               const ServiceWorkerVersionAttributes& attrs);
  void OnDisassociateRegistration();
  void OnServiceWorkerStateChanged(int handle_id,
                                   blink::WebServiceWorkerState state);
  void OnSetControllerServiceWorker(const ServiceWorkerObjectInfo& info);

  int provider_id() const { return provider_id_; }

  ServiceWorkerHandleReference* controller();

  bool GetRegistrationInfoAndVersionAttributes(
      ServiceWorkerRegistrationObjectInfo* info,
      ServiceWorkerVersionAttributes* attrs);
  void SetVersionAttributes(ChangedVersionAttributesMask mask,
                            const ServiceWorkerVersionAttributes& attrs);

  // Gets the handle ID of the installing Service Worker, or
  // kInvalidServiceWorkerHandleId if the provider does not have a
  // installing Service Worker.
  int installing_handle_id() const;

  // Gets the handle ID of the waiting Service Worker, or
  // kInvalidServiceWorkerHandleId if the provider does not have a
  // waiting Service Worker.
  int waiting_handle_id() const;

  // Gets the handle ID of the active Service Worker, or
  // kInvalidServiceWorkerHandleId if the provider does not have an active
  // Service Worker.
  int active_handle_id() const;

  // Gets the handle ID of the controller Service Worker, or
  // kInvalidServiceWorkerHandleId if the provider is not controlled
  // by a Service Worker.
  int controller_handle_id() const;

  // Gets the handle ID of the associated registration, or
  // kInvalidRegistrationHandleId if the provider is not associated with any
  // registration.
  int registration_handle_id() const;

 private:
  friend class base::DeleteHelper<ServiceWorkerProviderContext>;
  friend class base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                          ServiceWorkerProviderContextDeleter>;
  friend struct ServiceWorkerProviderContextDeleter;

  ~ServiceWorkerProviderContext();
  void DestructOnMainThread() const;

  const int provider_id_;
  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ServiceWorkerMessageSender> sender_;

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
