// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"

namespace blink {
class WebServiceWorkerNetworkProvider;
}  // namespace blink

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

namespace mojom {
class URLLoaderFactory;
}

struct CommitNavigationParams;
class RenderFrameImpl;
class ServiceWorkerProviderContext;

// ServiceWorkerNetworkProvider enables the browser process to recognize
// resource requests from Blink that should be handled by service worker
// machinery rather than the usual network stack.
//
// All requests associated with a network provider are tagged with its
// |provider_id| (from ServiceWorkerProviderContext). The browser
// process can then route the request to this provider's corresponding
// ServiceWorkerProviderHost.
//
// It is created for service worker clients (documents and shared workers). It
// is instantiated prior to the main resource load being started and remains
// allocated until after the last subresource load has occurred. It is owned by
// the appropriate DocumentLoader for the provider (i.e., the loader for a
// document, or the shadow page's loader for a shared worker).
// Each request coming from the DocumentLoader is tagged with the provider_id in
// WillSendRequest.
class CONTENT_EXPORT ServiceWorkerNetworkProvider {
 public:
  // Creates a ServiceWorkerNetworkProvider for navigation and wraps it
  // with WebServiceWorkerNetworkProvider to be owned by Blink.
  //
  // |commit_params| are navigation parameters that were transmitted to the
  // renderer by the browser on a navigation commit. It is null if we have not
  // yet heard from the browser (currently only during the time it takes from
  // having the renderer initiate a navigation until the browser commits it).
  // Note: in particular, provisional load failure do not provide
  // |commit_params|.
  // TODO(ahemery): Update this comment when do not create placeholder document
  // loaders for renderer-initiated navigations. In this case, this should never
  // be null.
  //
  // For S13nServiceWorker:
  // |controller_info| contains the endpoint and object info that is needed to
  // set up the controller service worker for the client.
  // |fallback_loader_factory| is a default loader factory for fallback
  // requests, and is used when we create a subresource loader for controllees.
  // This is non-null only if the provider is created for controllees, and if
  // the loading context, e.g. a frame, provides it.
  static std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
  CreateForNavigation(
      RenderFrameImpl* frame,
      const CommitNavigationParams* commit_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory);

  // Creates a "null" network provider for a frame (it has an invalid provider
  // id). Useful when the frame should not use service worker.
  static std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
  CreateInvalidInstanceForNavigation();

  // Creates a ServiceWorkerNetworkProvider for a shared worker (as a
  // non-document service worker client).
  //
  // For NetworkService (PlzWorker):
  // |controller_info| contains the endpoint and object info that is needed to
  // set up the controller service worker for the client.
  static std::unique_ptr<ServiceWorkerNetworkProvider> CreateForSharedWorker(
      blink::mojom::ServiceWorkerProviderInfoForSharedWorkerPtr info,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo
          script_loader_factory_info,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory);

  static ServiceWorkerNetworkProvider* FromWebServiceWorkerNetworkProvider(
      blink::WebServiceWorkerNetworkProvider*);

  ~ServiceWorkerNetworkProvider();

  int provider_id() const;
  ServiceWorkerProviderContext* context() const { return context_.get(); }

  network::mojom::URLLoaderFactory* script_loader_factory() {
    return script_loader_factory_.get();
  }

  // Returns whether the context this provider is for is controlled by a service
  // worker.
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker() const;

  // Called when blink::IdlenessDetector emits its network idle signal.
  void DispatchNetworkQuiet();

 private:
  // Creates an invalid instance (provider_id() returns
  // kInvalidServiceWorkerProviderId).
  ServiceWorkerNetworkProvider();

  // |is_parent_frame_secure| is only relevant when the
  // |type| is kForWindow.
  //
  // For S13nServiceWorker:
  // See the comment at CreateForNavigation() for |controller_info| and
  // |fallback_loader_factory|.
  ServiceWorkerNetworkProvider(
      int route_id,
      blink::mojom::ServiceWorkerProviderType type,
      int provider_id,
      bool is_parent_frame_secure,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory);

  ServiceWorkerNetworkProvider(
      blink::mojom::ServiceWorkerProviderInfoForSharedWorkerPtr info,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo
          script_loader_factory_info,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory);

  // |context_| is null if |this| is an invalid instance, in which case there is
  // no connection to the browser process.
  scoped_refptr<ServiceWorkerProviderContext> context_;

  blink::mojom::ServiceWorkerDispatcherHostAssociatedPtr dispatcher_host_;

  // For shared worker contexts. The URL loader factory for loading the worker's
  // scripts.
  network::mojom::URLLoaderFactoryAssociatedPtr script_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNetworkProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_
