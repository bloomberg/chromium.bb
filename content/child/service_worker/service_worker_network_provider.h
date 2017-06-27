// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"

namespace blink {
class WebLocalFrame;
class WebServiceWorkerNetworkProvider;
}  // namespace blink

namespace content {

struct RequestNavigationParams;
class ServiceWorkerProviderContext;

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
  static std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
  CreateForNavigation(int route_id,
                      const RequestNavigationParams& request_params,
                      blink::WebLocalFrame* frame,
                      bool content_initiated);

  // Valid only for WebServiceWorkerNetworkProvider created by
  // CreateForNavigation.
  static ServiceWorkerNetworkProvider* FromWebServiceWorkerNetworkProvider(
      blink::WebServiceWorkerNetworkProvider*);

  // PlzNavigate
  // The |browser_provider_id| is initialized by the browser for navigations.
  ServiceWorkerNetworkProvider(int route_id,
                               ServiceWorkerProviderType type,
                               int browser_provider_id,
                               bool is_parent_frame_secure);
  // This is for service worker clients.
  ServiceWorkerNetworkProvider(int route_id,
                               ServiceWorkerProviderType type,
                               bool is_parent_frame_secure);
  // This is for controllers.
  explicit ServiceWorkerNetworkProvider(
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr info);

  ServiceWorkerNetworkProvider();
  ~ServiceWorkerNetworkProvider();

  int provider_id() const { return provider_id_; }
  ServiceWorkerProviderContext* context() const { return context_.get(); }

  bool IsControlledByServiceWorker() const;

 private:
  const int provider_id_;
  scoped_refptr<ServiceWorkerProviderContext> context_;
  mojom::ServiceWorkerDispatcherHostAssociatedPtr dispatcher_host_;
  mojom::ServiceWorkerProviderHostAssociatedPtr provider_host_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNetworkProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_
