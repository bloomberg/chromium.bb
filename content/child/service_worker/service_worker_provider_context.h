// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/child/child_url_loader_factory_getter.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

namespace mojom {
class URLLoaderFactory;
}

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
      public mojom::ServiceWorkerContainer {
 public:
  // |provider_id| is used to identify this provider in IPC messages to the
  // browser process. |request| is an endpoint which is connected to
  // the content::ServiceWorkerProviderHost that notifies of changes to the
  // registration's and workers' status. |request| is bound with |binding_|.
  // The new instance is registered to |dispatcher|, which is not owned.
  //
  // For S13nServiceWorker:
  // |default_loader_factory_getter| contains a set of default loader
  // factories for the associated loading context, and is used when we
  // create a subresource loader for controllees. This is non-null only
  // if the provider is created for controllees, and if the loading context,
  // e.g. a frame, provides the default URLLoaderFactoryGetter.
  ServiceWorkerProviderContext(
      int provider_id,
      ServiceWorkerProviderType provider_type,
      mojom::ServiceWorkerContainerAssociatedRequest request,
      mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info,
      ServiceWorkerDispatcher* dispatcher,
      scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter);

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

  // S13nServiceWorker:
  // For service worker clients. Returns URLLoaderFactory for loading
  // subresources with the controller ServiceWorker.
  mojom::URLLoaderFactory* subresource_loader_factory();

  // For service worker clients. Keeps track of feature usage for UseCounter.
  void CountFeature(uint32_t feature);
  const std::set<uint32_t>& used_features() const;

  // For service worker clients. Creates a ServiceWorkerWorkerClientRequest
  // which can be used to bind with a WorkerFetchContextImpl in a (dedicated or
  // shared) worker thread and receive SetControllerServiceWorker() method call
  // from the main thread.
  // A dedicated worker's WorkerFetchContext calls CreateWorkerClientRequest()
  // on its parent Document's ServiceWorkerProviderContext. A shared worker's
  // fetch context calls CreateWorkerClientRequest() on its own
  // ServiceWorkerProviderContext.
  mojom::ServiceWorkerWorkerClientRequest CreateWorkerClientRequest();

  // Called when ServiceWorkerNetworkProvider is destructed. This function
  // severs the Mojo binding to the browser-side ServiceWorkerProviderHost. The
  // reason ServiceWorkerNetworkProvider is special compared to the other
  // providers, is that it is destructed synchronously when a service worker
  // client (Document) is removed from the DOM. Once this happens, the
  // ServiceWorkerProviderHost must destruct quickly in order to remove the
  // ServiceWorkerClient from the system (thus allowing unregistration/update to
  // occur and ensuring the Clients API doesn't return the client).
  void OnNetworkProviderDestroyed();

 private:
  friend class base::DeleteHelper<ServiceWorkerProviderContext>;
  friend class base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                          ServiceWorkerProviderContextDeleter>;
  friend struct ServiceWorkerProviderContextDeleter;
  struct ControlleeState;
  struct ControllerState;

  ~ServiceWorkerProviderContext() override;
  void DestructOnMainThread() const;

  // Clears the information of the ServiceWorkerWorkerClient of dedicated (or
  // shared) worker, when the connection to the worker is disconnected.
  void UnregisterWorkerFetchContext(mojom::ServiceWorkerWorkerClient*);

  const int provider_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Mojo binding for the |request| passed to the constructor. This keeps the
  // connection to the content::ServiceWorkerProviderHost in the browser process
  // alive.
  mojo::AssociatedBinding<mojom::ServiceWorkerContainer> binding_;
  // |container_host_| is the Mojo endpoint to the browser-side
  // mojom::ServiceWorkerContainerHost that hosts this
  // mojom::ServiceWorkerContainer.
  mojom::ServiceWorkerContainerHostAssociatedPtr container_host_;

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
