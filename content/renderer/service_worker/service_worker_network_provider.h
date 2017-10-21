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
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"

namespace blink {
class WebLocalFrame;
class WebServiceWorkerNetworkProvider;
}  // namespace blink

namespace content {

namespace mojom {
class URLLoaderFactory;
}

struct RequestNavigationParams;
class ServiceWorkerProviderContext;
class ChildURLLoaderFactoryGetter;

// A unique provider_id is generated for each instance.
// Instantiated prior to the main resource load being started and remains
// allocated until after the last subresource load has occurred.
// This is used to track the lifetime of a Document to create
// and dispose the ServiceWorkerProviderHost in the browser process
// to match its lifetime. Each request coming from the Document is
// tagged with this id in willSendRequest.
//
// Basically, it's a scoped integer that sends an ipc upon construction
// and destruction.
class CONTENT_EXPORT ServiceWorkerNetworkProvider {
 public:
  // Creates a ServiceWorkerNetworkProvider for navigation and wraps it
  // with WebServiceWorkerNetworkProvider to be owned by Blink.
  //
  // For S13nServiceWorker:
  // |default_loader_factory_getter| contains a set of default loader
  // factories for the associated loading context, and is used when we
  // create a subresource loader for controllees. This is non-null only
  // if the provider is created for controllees, and if the loading context,
  // e.g. a frame, provides the loading factory getter for default loaders.
  static std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
  CreateForNavigation(
      int route_id,
      const RequestNavigationParams& request_params,
      blink::WebLocalFrame* frame,
      bool content_initiated,
      scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter);

  // Creates a ServiceWorkerNetworkProvider for a shared worker (as a
  // non-document service worker client).
  // TODO(kinuko): This should also take ChildURLLoaderFactoryGetter associated
  // with the SharedWorker.
  static std::unique_ptr<ServiceWorkerNetworkProvider> CreateForSharedWorker(
      int route_id);

  // Creates a ServiceWorkerNetworkProvider for a "controller" (i.e.
  // a service worker execution context).
  static std::unique_ptr<ServiceWorkerNetworkProvider> CreateForController(
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr info);

  // Valid only for WebServiceWorkerNetworkProvider created by
  // CreateForNavigation.
  static ServiceWorkerNetworkProvider* FromWebServiceWorkerNetworkProvider(
      blink::WebServiceWorkerNetworkProvider*);

  ~ServiceWorkerNetworkProvider();

  int provider_id() const;
  ServiceWorkerProviderContext* context() const { return context_.get(); }

  mojom::URLLoaderFactory* script_loader_factory() {
    return script_loader_factory_.get();
  }

  bool IsControlledByServiceWorker() const;

 private:
  ServiceWorkerNetworkProvider();

  // This is for service worker clients (used in CreateForNavigation and
  // CreateForSharedWorker). |provider_id| is provided by the browser process
  // for navigations (with PlzNavigate, which is default).
  // |type| must be either one of SERVICE_WORKER_PROVIDER_FOR_{WINDOW,
  // SHARED_WORKER,WORKER} (while currently we don't have code for WORKER).
  // |is_parent_frame_secure| is only relevant when the |type| is WINDOW.
  //
  // For S13nServiceWorker:
  // |default_loader_factory_getter| contains a set of default loader
  // factories for the associated loading context, and is used when we
  // create a subresource loader for controllees. This is non-null only
  // if the provider is created for controllees, and if the loading context,
  // e.g. a frame, provides the loading factory getter for default loaders.
  ServiceWorkerNetworkProvider(
      int route_id,
      ServiceWorkerProviderType type,
      int provider_id,
      bool is_parent_frame_secure,
      scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter);

  // This is for controllers, used in CreateForController.
  explicit ServiceWorkerNetworkProvider(
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr info);

  scoped_refptr<ServiceWorkerProviderContext> context_;
  mojom::ServiceWorkerDispatcherHostAssociatedPtr dispatcher_host_;
  mojom::URLLoaderFactoryAssociatedPtr script_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNetworkProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_
