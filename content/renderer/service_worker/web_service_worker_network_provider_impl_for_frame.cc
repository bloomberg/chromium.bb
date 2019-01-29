// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_network_provider_impl_for_frame.h"

#include <utility>

#include "content/public/common/origin_util.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/service_worker_network_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

class WebServiceWorkerNetworkProviderImplForFrame::NewDocumentObserver
    : public RenderFrameObserver {
 public:
  NewDocumentObserver(WebServiceWorkerNetworkProviderImplForFrame* owner,
                      RenderFrameImpl* frame)
      : RenderFrameObserver(frame), owner_(owner) {}

  void DidCreateNewDocument() override {
    blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
    blink::WebDocumentLoader* web_loader =
        render_frame()->GetWebFrame()->GetDocumentLoader();
    DCHECK_EQ(owner_, web_loader->GetServiceWorkerNetworkProvider());

    if (web_frame->GetSecurityOrigin().IsOpaque()) {
      // At navigation commit we thought the document was eligible to use
      // service workers so created the network provider, but it turns out it is
      // not eligible because it is CSP sandboxed.
      web_loader->SetServiceWorkerNetworkProvider(
          ServiceWorkerNetworkProvider::CreateInvalidInstanceForNavigation());
      // |this| and its owner are destroyed.
      return;
    }

    owner_->NotifyExecutionReady();
  }

  void OnDestruct() override {
    // Deletes |this|.
    owner_->observer_.reset();
  }

 private:
  WebServiceWorkerNetworkProviderImplForFrame* owner_;
};

WebServiceWorkerNetworkProviderImplForFrame::
    WebServiceWorkerNetworkProviderImplForFrame(
        std::unique_ptr<ServiceWorkerNetworkProvider> provider,
        RenderFrameImpl* frame)
    : WebServiceWorkerNetworkProviderBaseImpl(std::move(provider)) {
  if (frame)
    observer_ = std::make_unique<NewDocumentObserver>(this, frame);
}

WebServiceWorkerNetworkProviderImplForFrame::
    ~WebServiceWorkerNetworkProviderImplForFrame() = default;

void WebServiceWorkerNetworkProviderImplForFrame::WillSendRequest(
    blink::WebURLRequest& request) {
  if (!request.GetExtraData())
    request.SetExtraData(std::make_unique<RequestExtraData>());
  auto* extra_data = static_cast<RequestExtraData*>(request.GetExtraData());
  extra_data->set_service_worker_provider_id(provider()->provider_id());

  // If the provider does not have a controller at this point, the renderer
  // expects the request to never be handled by a service worker, so call
  // SetSkipServiceWorker() with true to skip service workers here. Otherwise,
  // a service worker that is in the process of becoming the controller (i.e.,
  // via claim()) on the browser-side could handle the request and break the
  // assumptions of the renderer.
  if (request.GetFrameType() !=
          network::mojom::RequestContextFrameType::kTopLevel &&
      request.GetFrameType() !=
          network::mojom::RequestContextFrameType::kNested &&
      IsControlledByServiceWorker() ==
          blink::mojom::ControllerServiceWorkerMode::kNoController) {
    request.SetSkipServiceWorker(true);
  }

  // Inject this frame's fetch window id into the request. This is really only
  // needed for subresource requests in S13nServiceWorker. For main resource
  // requests or non-S13nSW case, the browser process sets the id on the
  // request when dispatching the fetch event to the service worker. But it
  // doesn't hurt to set it always.
  if (provider()->context())
    request.SetFetchWindowId(provider()->context()->fetch_request_window_id());
}

std::unique_ptr<blink::WebURLLoader>
WebServiceWorkerNetworkProviderImplForFrame::CreateURLLoader(
    const blink::WebURLRequest& request,
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
        task_runner_handle) {
  // RenderThreadImpl is nullptr in some tests.
  if (!RenderThreadImpl::current())
    return nullptr;

  // S13nServiceWorker:
  // We only install our own URLLoader if Servicification is enabled.
  if (!blink::ServiceWorkerUtils::IsServicificationEnabled())
    return nullptr;

  // We need SubresourceLoaderFactory populated in order to create our own
  // URLLoader for subresource loading.
  if (!provider()->context() ||
      !provider()->context()->GetSubresourceLoaderFactory())
    return nullptr;

  // If the URL is not http(s) or otherwise whitelisted, do not intercept the
  // request. Schemes like 'blob' and 'file' are not eligible to be intercepted
  // by service workers.
  // TODO(falken): Let ServiceWorkerSubresourceLoaderFactory handle the request
  // and move this check there (i.e., for such URLs, it should use its fallback
  // factory).
  const GURL gurl(request.Url());
  if (!gurl.SchemeIsHTTPOrHTTPS() && !OriginCanAccessServiceWorkers(gurl))
    return nullptr;

  // If GetSkipServiceWorker() returns true, do not intercept the request.
  if (request.GetSkipServiceWorker())
    return nullptr;

  // Create our own SubresourceLoader to route the request to the controller
  // ServiceWorker.
  // TODO(crbug.com/796425): Temporarily wrap the raw mojom::URLLoaderFactory
  // pointer into SharedURLLoaderFactory.
  return std::make_unique<WebURLLoaderImpl>(
      RenderThreadImpl::current()->resource_dispatcher(),
      std::move(task_runner_handle),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          provider()->context()->GetSubresourceLoaderFactory()));
}

void WebServiceWorkerNetworkProviderImplForFrame::DispatchNetworkQuiet() {
  provider()->DispatchNetworkQuiet();
}

void WebServiceWorkerNetworkProviderImplForFrame::NotifyExecutionReady() {
  if (provider()->context())
    provider()->context()->NotifyExecutionReady();
}

}  // namespace content
