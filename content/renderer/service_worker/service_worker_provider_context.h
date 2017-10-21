// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/child/child_url_loader_factory_getter.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

namespace mojom {
class URLLoaderFactory;
}

class ServiceWorkerHandleReference;
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

  ServiceWorkerProviderType provider_type() const { return provider_type_; }

  int provider_id() const { return provider_id_; }

  // For service worker execution contexts. Sets the registration for
  // ServiceWorkerGlobalScope#registration. Unlike
  // TakeRegistrationForServiceWorkerGlobalScope(), called on the main thread.
  // SetRegistrationForServiceWorkerGlobalScope() is called during the setup for
  // service worker startup, so it is guaranteed to be called before
  // TakeRegistrationForServiceWorkerGlobalScope().
  void SetRegistrationForServiceWorkerGlobalScope(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration,
      std::unique_ptr<ServiceWorkerHandleReference> installing,
      std::unique_ptr<ServiceWorkerHandleReference> waiting,
      std::unique_ptr<ServiceWorkerHandleReference> active);

  // For service worker execution contexts. Used for initializing
  // ServiceWorkerGlobalScope#registration. Called on the worker thread.
  // This takes ServiceWorkerRegistrationObjectHost ptr info from
  // ControllerState::registration.
  void TakeRegistrationForServiceWorkerGlobalScope(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr* info,
      ServiceWorkerVersionAttributes* attrs);

  // For service worker clients. The controller for
  // ServiceWorkerContainer#controller.
  void SetController(std::unique_ptr<ServiceWorkerHandleReference> controller,
                     const std::set<uint32_t>& used_features);
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

  // Gets the mojom::ServiceWorkerContainerHost* for sending requests to
  // browser-side ServiceWorkerProviderHost. May be nullptr if
  // OnNetworkProviderDestroyed() has already been called.
  // Currently this can be called only for clients that are Documents,
  // see comments of |container_host_|.
  mojom::ServiceWorkerContainerHost* container_host() const;

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

  const ServiceWorkerProviderType provider_type_;
  const int provider_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Mojo binding for the |request| passed to the constructor. This keeps the
  // connection to the content::ServiceWorkerProviderHost in the browser process
  // alive.
  mojo::AssociatedBinding<mojom::ServiceWorkerContainer> binding_;

  // The |container_host_| interface represents the connection to the
  // browser-side ServiceWorkerProviderHost, whose lifetime is bound to
  // |container_host_| via the Mojo connection.
  // The |container_host_| interface also implements functions for
  // navigator.serviceWorker, but all the methods that correspond to
  // navigator.serviceWorker.* can be used only if |this| is a provider
  // for a Document, as navigator.serviceWorker is currently only implemented
  // for Document (https://crbug.com/371690).
  // Note: Currently this is always bound on main thread.
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

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
