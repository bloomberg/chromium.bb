// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_provider_interfaces.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class ServiceWorkerHandleReference;
class ServiceWorkerRegistrationHandleReference;
struct ServiceWorkerProviderContextDeleter;

// ServiceWorkerProviderContext stores common state for service worker
// "providers" (currently WebServiceWorkerProviderImpl,
// ServiceWorkerNetworkProvider, and ServiceWorkerContextClient). Providers for
// the same underlying entity hold strong references to a shared instance of
// this class.
//
// The ServiceWorkerProviderContext has different roles depending on if it's for
// a "controllee" (a Document or Worker execution context), or a "controller" (a
// service worker execution context).
//  - For controllees, it's used for keeping the controller alive to create
//    controllee's ServiceWorkerContainer#controller. The reference to the
//    controller is kept until SetController() is called with an
//    invalid worker info.
//  - For controllers, it's used for keeping the associated registration and
//    its versions alive to create the controller's
//    ServiceWorkerGlobalScope#registration.
//
// Created and destructed on the main thread. Unless otherwise noted, all
// methods are called on the main thread.
class CONTENT_EXPORT ServiceWorkerProviderContext
    : public base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                        ServiceWorkerProviderContextDeleter>,
      public mojom::ServiceWorkerProvider {
 public:
  // |provider_id| is used to identify this provider in IPC messages to the
  // browser process. |request| is an endpoint which is connected to
  // the content::ServiceWorkerProviderHost that notifies of changes to the
  // registration's and workers' status. |request| is bound with |binding_|.
  // The new instance is registered to |dispatcher|, which is not owned.
  ServiceWorkerProviderContext(
      int provider_id,
      ServiceWorkerProviderType provider_type,
      mojom::ServiceWorkerProviderAssociatedRequest request,
      ServiceWorkerDispatcher* dispatcher);

  int provider_id() const { return provider_id_; }

  // For service worker execution contexts. Sets the registration for
  // ServiceWorkerGlobalScope#registration. Unlike GetRegistration(),
  // called on the main thread. SetRegistration() is called during
  // the setup for service worker startup, so it is guaranteed to be called
  // before GetRegistration().
  void SetRegistration(
      std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration,
      std::unique_ptr<ServiceWorkerHandleReference> installing,
      std::unique_ptr<ServiceWorkerHandleReference> waiting,
      std::unique_ptr<ServiceWorkerHandleReference> active);

  // For service worker execution contexts. Used for initializing
  // ServiceWorkerGlobalScope#registration. Called on the worker thread.
  void GetRegistration(ServiceWorkerRegistrationObjectInfo* info,
                       ServiceWorkerVersionAttributes* attrs);

  // For service worker clients. The controller for
  // ServiceWorkerContainer#controller.
  void SetController(
      std::unique_ptr<ServiceWorkerHandleReference> controller,
      const std::set<uint32_t>& used_features,
      mojom::ServiceWorkerEventDispatcherPtrInfo event_dispatcher_ptr_info);
  ServiceWorkerHandleReference* controller();

  // For service worker clients. Gets the ServiceWorkerEventDispatcher
  // for dispatching events to the controller ServiceWorker.
  mojom::ServiceWorkerEventDispatcher* event_dispatcher();

  // For service worker clients. Keeps track of feature usage for UseCounter.
  void CountFeature(uint32_t feature);
  const std::set<uint32_t>& used_features() const;

 private:
  friend class base::DeleteHelper<ServiceWorkerProviderContext>;
  friend class base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                          ServiceWorkerProviderContextDeleter>;
  friend struct ServiceWorkerProviderContextDeleter;
  struct ControlleeState;
  struct ControllerState;

  ~ServiceWorkerProviderContext() override;
  void DestructOnMainThread() const;

  const int provider_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Mojo binding for the |request| passed to the constructor. This keeps the
  // connection to the content::ServiceWorkerProviderHost in the browser process
  // alive.
  mojo::AssociatedBinding<mojom::ServiceWorkerProvider> binding_;

  // Either |controllee_state_| or |controller_state_| is non-null.
  std::unique_ptr<ControlleeState> controllee_state_;
  std::unique_ptr<ControllerState> controller_state_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContext);
};

struct ServiceWorkerProviderContextDeleter {
  static void Destruct(const ServiceWorkerProviderContext* context) {
    context->DestructOnMainThread();
  }
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
