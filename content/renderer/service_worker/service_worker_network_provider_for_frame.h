// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_FRAME_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_FRAME_H_

#include <memory>

#include "content/common/content_export.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"

namespace content {
struct CommitNavigationParams;
class RenderFrameImpl;

// The WebServiceWorkerNetworkProvider implementation used for frames.
class CONTENT_EXPORT ServiceWorkerNetworkProviderForFrame final
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  // Creates a network provider for |frame|.
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
  static std::unique_ptr<ServiceWorkerNetworkProviderForFrame> Create(
      RenderFrameImpl* frame,
      const CommitNavigationParams* commit_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory);

  // Creates an invalid instance. It has a null |context()|.
  // TODO(falken): Just use null instead of this.
  static std::unique_ptr<ServiceWorkerNetworkProviderForFrame>
  CreateInvalidInstance();

  ~ServiceWorkerNetworkProviderForFrame() override;

  // Implements WebServiceWorkerNetworkProvider.
  void WillSendRequest(blink::WebURLRequest& request) override;
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker()
      override;
  int64_t ControllerServiceWorkerID() override;
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override;
  void DispatchNetworkQuiet() override;

  int provider_id() const;
  ServiceWorkerProviderContext* context() { return context_.get(); }

 private:
  class NewDocumentObserver;

  explicit ServiceWorkerNetworkProviderForFrame(RenderFrameImpl* frame);

  void NotifyExecutionReady();

  // |context_| is null if |this| is an invalid instance, in which case there is
  // no connection to the browser process.
  scoped_refptr<ServiceWorkerProviderContext> context_;

  blink::mojom::ServiceWorkerDispatcherHostAssociatedPtr dispatcher_host_;

  std::unique_ptr<NewDocumentObserver> observer_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_FRAME_H_
