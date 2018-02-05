// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/controller_service_worker.mojom.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/common/service_worker/service_worker_object.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

namespace mojom {
class URLLoaderFactory;
}  // namespace mojom

namespace service_worker_provider_context_unittest {
class ServiceWorkerProviderContextTest;
}  // namespace service_worker_provider_context_unittest

class WebServiceWorkerRegistrationImpl;
struct ServiceWorkerProviderContextDeleter;

// ServiceWorkerProviderContext stores common state for service worker
// "providers" (currently WebServiceWorkerProviderImpl,
// ServiceWorkerNetworkProvider, and ServiceWorkerContextClient). Providers for
// the same underlying entity hold strong references to a shared instance of
// this class.
//
// A service worker provider may exist for either a service worker client or a
// service worker itself. Therefore, this class has different roles depending on
// its provider type. See the implementation of ProviderStateForClient and
// ProviderStateForServiceWorker for details.
//
// Created and destructed on the main thread. Unless otherwise noted, all
// methods are called on the main thread.
class CONTENT_EXPORT ServiceWorkerProviderContext
    : public base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                        ServiceWorkerProviderContextDeleter>,
      public mojom::ServiceWorkerContainer {
 public:
  // Constructor for service worker clients.
  //
  // |provider_id| is used to identify this provider in IPC messages to the
  // browser process. |request| is an endpoint which is connected to
  // the content::ServiceWorkerProviderHost that notifies of changes to the
  // registration's and workers' status. |request| is bound with |binding_|.
  //
  // For S13nServiceWorker:
  // |controller_info| contains the endpoint and object info that is needed to
  // set up the controller service worker for the client.
  // |default_loader_factory| is a default loader factory for network requests,
  // and is used when we create a subresource loader for controllees. This is
  // non-null only if the provider is created for controllees, and if the
  // loading context, e.g. a frame, provides it.
  ServiceWorkerProviderContext(
      int provider_id,
      blink::mojom::ServiceWorkerProviderType provider_type,
      mojom::ServiceWorkerContainerAssociatedRequest request,
      mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info,
      mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<SharedURLLoaderFactory> default_loader_factory);

  // Constructor for service worker execution contexts.
  ServiceWorkerProviderContext(
      int provider_id,
      mojom::ServiceWorkerContainerAssociatedRequest request,
      mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info);

  blink::mojom::ServiceWorkerProviderType provider_type() const {
    return provider_type_;
  }

  int provider_id() const { return provider_id_; }

  // For service worker execution contexts. Sets the registration for
  // ServiceWorkerGlobalScope#registration. Unlike
  // TakeRegistrationForServiceWorkerGlobalScope(), called on the main thread.
  // SetRegistrationForServiceWorkerGlobalScope() is called during the setup for
  // service worker startup, so it is guaranteed to be called before
  // TakeRegistrationForServiceWorkerGlobalScope().
  void SetRegistrationForServiceWorkerGlobalScope(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration);

  // For service worker execution contexts. Used for initializing
  // ServiceWorkerGlobalScope#registration. Called on the worker thread.
  // This takes the registration that was passed to
  // SetRegistrationForServiceWorkerScope(), then creates a new
  // WebServiceWorkerRegistrationImpl instance and returns it. |io_task_runner|
  // is used to initialize WebServiceWorkerRegistrationImpl.
  scoped_refptr<WebServiceWorkerRegistrationImpl>
  TakeRegistrationForServiceWorkerGlobalScope(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // For service worker clients. Returns version id of the controller service
  // worker object (ServiceWorkerContainer#controller).
  int64_t GetControllerVersionId();

  // For service worker clients. Takes the controller service worker object info
  // set by SetController() if any, otherwise returns nullptr.
  blink::mojom::ServiceWorkerObjectInfoPtr TakeController();

  // S13nServiceWorker:
  // For service worker clients. Returns URLLoaderFactory for loading
  // subresources with the controller ServiceWorker, or nullptr if
  // no controller is attached.
  network::mojom::URLLoaderFactory* GetSubresourceLoaderFactory();

  // For service worker clients. Returns the feature usage of its controller.
  const std::set<blink::mojom::WebFeature>& used_features() const;

  // For service worker clients. Sets a weak pointer back to the
  // WebServiceWorkerProviderImpl (which corresponds to ServiceWorkerContainer
  // in JavaScript) which has a strong reference to |this|. This allows us to
  // notify the WebServiceWorkerProviderImpl when
  // ServiceWorkerContainer#controller should be changed.
  void SetWebServiceWorkerProvider(
      base::WeakPtr<WebServiceWorkerProviderImpl> provider);

  // For service worker clients. Creates a ServiceWorkerWorkerClientRequest
  // which can be used to bind with a WorkerFetchContextImpl in a (dedicated or
  // shared) worker thread and receive SetControllerServiceWorker() method call
  // from the main thread.
  // A dedicated worker's WorkerFetchContext calls CreateWorkerClientRequest()
  // on its parent Document's ServiceWorkerProviderContext. A shared worker's
  // fetch context calls CreateWorkerClientRequest() on its own
  // ServiceWorkerProviderContext.
  mojom::ServiceWorkerWorkerClientRequest CreateWorkerClientRequest();

  // S13nServiceWorker:
  // For service worker clients. Creates a ServiceWorkerContainerHostPtrInfo
  // which can be bound to a ServiceWorkerContainerHostPtr in a (dedicated or
  // shared) worker thread. WorkerFetchContextImpl will use the host pointer to
  // get the controller service worker by GetControllerServiceWorker() and send
  // FetchEvents to the service worker.
  mojom::ServiceWorkerContainerHostPtrInfo CloneContainerHostPtrInfo();

  // For service worker clients. Returns the registration object described by
  // |info|. Creates a new object if needed, or else returns the existing one.
  scoped_refptr<WebServiceWorkerRegistrationImpl>
  GetOrCreateRegistrationForServiceWorkerClient(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info);

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
  friend class service_worker_provider_context_unittest::
      ServiceWorkerProviderContextTest;
  friend class WebServiceWorkerRegistrationImpl;
  friend struct ServiceWorkerProviderContextDeleter;
  struct ProviderStateForClient;
  struct ProviderStateForServiceWorker;

  ~ServiceWorkerProviderContext() override;
  void DestructOnMainThread() const;

  // Clears the information of the ServiceWorkerWorkerClient of dedicated (or
  // shared) worker, when the connection to the worker is disconnected.
  void UnregisterWorkerFetchContext(mojom::ServiceWorkerWorkerClient*);

  // Implementation of mojom::ServiceWorkerContainer.
  void SetController(mojom::ControllerServiceWorkerInfoPtr controller_info,
                     const std::vector<blink::mojom::WebFeature>& used_features,
                     bool should_notify_controllerchange) override;
  void PostMessageToClient(
      blink::mojom::ServiceWorkerObjectInfoPtr source,
      const base::string16& message,
      std::vector<mojo::ScopedMessagePipeHandle> message_pipes) override;
  void CountFeature(blink::mojom::WebFeature feature) override;

  // For service worker clients. Keeps the mapping from registration_id to
  // ServiceWorkerRegistration object.
  void AddServiceWorkerRegistration(
      int64_t registration_id,
      WebServiceWorkerRegistrationImpl* registration);
  void RemoveServiceWorkerRegistration(int64_t registration_id);
  bool ContainsServiceWorkerRegistrationForTesting(int64_t registration_id);

  // S13nServiceWorker:
  // For service worker clients.
  // A convenient utility method to tell if a subresource loader factory
  // can be created for this client.
  bool CanCreateSubresourceLoaderFactory() const;

  const blink::mojom::ServiceWorkerProviderType provider_type_;
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

  // Either |state_for_client_| or |state_for_service_worker_| is non-null.
  // State for service worker clients.
  std::unique_ptr<ProviderStateForClient> state_for_client_;
  // State for service workers.
  std::unique_ptr<ProviderStateForServiceWorker> state_for_service_worker_;

  // NOTE: New members should usually be added to either
  // |state_for_service_worker_| or |state_for_client_|. Not here!

  base::WeakPtrFactory<ServiceWorkerProviderContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContext);
};

struct ServiceWorkerProviderContextDeleter {
  static void Destruct(const ServiceWorkerProviderContext* context) {
    context->DestructOnMainThread();
  }
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
