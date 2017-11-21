// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_network_provider.h"

#include "base/atomic_sequence_num.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/navigation_params.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_provider_host_info.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/renderer/child_url_loader_factory_getter.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_handle_reference.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "third_party/WebKit/common/sandbox_flags.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerNetworkProvider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_object.mojom.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

namespace {

// Must be unique in the child process.
int GetNextProviderId() {
  static base::AtomicSequenceNumber sequence;
  return sequence.GetNext();  // We start at zero.
}

// Returns whether it's possible for a document whose frame is a descendant of
// |frame| to be a secure context, not considering scheme exceptions (since any
// document can be a secure context if it has a scheme exception). See
// Document::isSecureContextImpl for more details.
bool IsFrameSecure(blink::WebFrame* frame) {
  while (frame) {
    if (!frame->GetSecurityOrigin().IsPotentiallyTrustworthy())
      return false;
    frame = frame->Parent();
  }
  return true;
}

// An WebServiceWorkerNetworkProvider for frame. This wraps
// ServiceWorkerNetworkProvider implementation and is owned by blink.
class WebServiceWorkerNetworkProviderForFrame
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  WebServiceWorkerNetworkProviderForFrame(
      std::unique_ptr<ServiceWorkerNetworkProvider> provider)
      : provider_(std::move(provider)) {}

  void WillSendRequest(blink::WebURLRequest& request) override {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData());
    if (!extra_data)
      extra_data = new RequestExtraData();
    extra_data->set_service_worker_provider_id(provider_->provider_id());
    request.SetExtraData(extra_data);

    // If the provider does not have a controller at this point, the renderer
    // expects the request to never be handled by a controlling service worker,
    // so set the ServiceWorkerMode to skip local workers here. Otherwise, a
    // service worker that is in the process of becoming the controller (i.e.,
    // via claim()) on the browser-side could handle the request and break
    // the assumptions of the renderer.
    if (request.GetFrameType() != blink::WebURLRequest::kFrameTypeTopLevel &&
        request.GetFrameType() != blink::WebURLRequest::kFrameTypeNested &&
        !provider_->IsControlledByServiceWorker() &&
        request.GetServiceWorkerMode() !=
            blink::WebURLRequest::ServiceWorkerMode::kNone) {
      request.SetServiceWorkerMode(
          blink::WebURLRequest::ServiceWorkerMode::kForeign);
    }
  }

  int ProviderID() const override { return provider_->provider_id(); }

  bool HasControllerServiceWorker() override {
    return provider_->IsControlledByServiceWorker();
  }

  int64_t ControllerServiceWorkerID() override {
    if (provider_->context())
      return provider_->context()->GetControllerVersionId();
    return blink::mojom::kInvalidServiceWorkerVersionId;
  }

  ServiceWorkerNetworkProvider* provider() { return provider_.get(); }

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    // RenderThreadImpl is nullptr in some tests.
    if (!RenderThreadImpl::current())
      return nullptr;

    // S13nServiceWorker:
    // We only install our own URLLoader if Servicification is
    // enabled.
    if (!ServiceWorkerUtils::IsServicificationEnabled())
      return nullptr;

    // S13nServiceWorker:
    // We need SubresourceLoaderFactory populated in order to
    // create our own URLLoader for subresource loading.
    if (!provider_->context() ||
        !provider_->context()->subresource_loader_factory())
      return nullptr;

    // S13nServiceWorker:
    // If it's not for HTTP or HTTPS no need to intercept the
    // request.
    if (!GURL(request.Url()).SchemeIsHTTPOrHTTPS())
      return nullptr;

    // S13nServiceWorker:
    // If the service worker mode is not all, no need to intercept the request.
    if (request.GetServiceWorkerMode() !=
        blink::WebURLRequest::ServiceWorkerMode::kAll) {
      return nullptr;
    }

    // S13nServiceWorker:
    // Create our own SubresourceLoader to route the request
    // to the controller ServiceWorker.
    return std::make_unique<WebURLLoaderImpl>(
        RenderThreadImpl::current()->resource_dispatcher(),
        std::move(task_runner),
        provider_->context()->subresource_loader_factory());
  }

 private:
  std::unique_ptr<ServiceWorkerNetworkProvider> provider_;
};

}  // namespace

// static
std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
ServiceWorkerNetworkProvider::CreateForNavigation(
    int route_id,
    const RequestNavigationParams& request_params,
    blink::WebLocalFrame* frame,
    bool content_initiated,
    scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter) {
  bool browser_side_navigation = IsBrowserSideNavigationEnabled();
  bool should_create_provider_for_window = false;
  int service_worker_provider_id = kInvalidServiceWorkerProviderId;
  std::unique_ptr<ServiceWorkerNetworkProvider> network_provider;

  // Determine if a ServiceWorkerNetworkProvider should be created and properly
  // initialized for the navigation. A default ServiceWorkerNetworkProvider
  // will always be created since it is expected in a certain number of places,
  // however it will have an invalid id.
  // PlzNavigate: |service_worker_provider_id| can be sent by the browser, if
  // it already created the SeviceWorkerProviderHost.
  if (browser_side_navigation && !content_initiated) {
    should_create_provider_for_window =
        request_params.should_create_service_worker;
    service_worker_provider_id = request_params.service_worker_provider_id;
    DCHECK(ServiceWorkerUtils::IsBrowserAssignedProviderId(
               service_worker_provider_id) ||
           service_worker_provider_id == kInvalidServiceWorkerProviderId);
  } else {
    should_create_provider_for_window =
        ((frame->EffectiveSandboxFlags() & blink::WebSandboxFlags::kOrigin) !=
         blink::WebSandboxFlags::kOrigin);
  }

  // Now create the ServiceWorkerNetworkProvider (with invalid id if needed).
  if (should_create_provider_for_window) {
    // Ideally Document::isSecureContext would be called here, but the document
    // is not created yet, and due to redirects the URL may change. So pass
    // is_parent_frame_secure to the browser process, so it can determine the
    // context security when deciding whether to allow a service worker to
    // control the document.
    const bool is_parent_frame_secure = IsFrameSecure(frame->Parent());

    if (service_worker_provider_id == kInvalidServiceWorkerProviderId) {
      network_provider = base::WrapUnique(new ServiceWorkerNetworkProvider(
          route_id, SERVICE_WORKER_PROVIDER_FOR_WINDOW, GetNextProviderId(),
          is_parent_frame_secure, std::move(default_loader_factory_getter)));
    } else {
      CHECK(browser_side_navigation);
      DCHECK(ServiceWorkerUtils::IsBrowserAssignedProviderId(
          service_worker_provider_id));
      network_provider = base::WrapUnique(new ServiceWorkerNetworkProvider(
          route_id, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
          service_worker_provider_id, is_parent_frame_secure,
          std::move(default_loader_factory_getter)));
    }
  } else {
    network_provider = base::WrapUnique(new ServiceWorkerNetworkProvider());
  }
  return std::make_unique<WebServiceWorkerNetworkProviderForFrame>(
      std::move(network_provider));
}

// static
std::unique_ptr<ServiceWorkerNetworkProvider>
ServiceWorkerNetworkProvider::CreateForSharedWorker(int route_id) {
  // TODO(kinuko): Provide ChildURLLoaderFactoryGetter associated with
  // the SharedWorker.
  return base::WrapUnique(new ServiceWorkerNetworkProvider(
      route_id, SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER, GetNextProviderId(),
      true /* is_parent_frame_secure */,
      nullptr /* default_loader_factory_getter */));
}

// static
std::unique_ptr<ServiceWorkerNetworkProvider>
ServiceWorkerNetworkProvider::CreateForController(
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr info) {
  return base::WrapUnique(new ServiceWorkerNetworkProvider(std::move(info)));
}

// static
ServiceWorkerNetworkProvider*
ServiceWorkerNetworkProvider::FromWebServiceWorkerNetworkProvider(
    blink::WebServiceWorkerNetworkProvider* provider) {
  if (!provider) {
    DCHECK(ServiceWorkerUtils::IsServicificationEnabled());
    return nullptr;
  }
  return static_cast<WebServiceWorkerNetworkProviderForFrame*>(provider)
      ->provider();
}

ServiceWorkerNetworkProvider::~ServiceWorkerNetworkProvider() {
  if (context()) {
    context()->OnNetworkProviderDestroyed();
  }
}

int ServiceWorkerNetworkProvider::provider_id() const {
  if (!context())
    return kInvalidServiceWorkerProviderId;
  return context()->provider_id();
}

bool ServiceWorkerNetworkProvider::IsControlledByServiceWorker() const {
  return context() && context()->GetControllerVersionId() !=
                          blink::mojom::kInvalidServiceWorkerVersionId;
}

// Creates an invalid instance (provider_id() returns
// kInvalidServiceWorkerProviderId).
ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider() {}

// Constructor for service worker clients.
ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider(
    int route_id,
    ServiceWorkerProviderType provider_type,
    int browser_provider_id,
    bool is_parent_frame_secure,
    scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter) {
  if (browser_provider_id == kInvalidServiceWorkerProviderId)
    return;

  // We don't support dedicated worker (WORKER) as an independent service
  // worker client yet.
  DCHECK(provider_type == SERVICE_WORKER_PROVIDER_FOR_WINDOW ||
         provider_type == SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER);

  ServiceWorkerProviderHostInfo host_info(
      browser_provider_id, route_id, provider_type, is_parent_frame_secure);
  mojom::ServiceWorkerContainerAssociatedRequest client_request =
      mojo::MakeRequest(&host_info.client_ptr_info);
  mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  host_info.host_request = mojo::MakeRequest(&host_ptr_info);
  DCHECK(host_info.host_request.is_pending());
  DCHECK(host_info.host_request.handle().is_valid());

  // current() may be null in tests.
  if (ChildThreadImpl::current()) {
    ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
        ChildThreadImpl::current()->thread_safe_sender(),
        base::ThreadTaskRunnerHandle::Get().get());
    context_ = base::MakeRefCounted<ServiceWorkerProviderContext>(
        browser_provider_id, provider_type, std::move(client_request),
        std::move(host_ptr_info), default_loader_factory_getter);
    ChildThreadImpl::current()->channel()->GetRemoteAssociatedInterface(
        &dispatcher_host_);
    dispatcher_host_->OnProviderCreated(std::move(host_info));
  } else {
    context_ = base::MakeRefCounted<ServiceWorkerProviderContext>(
        browser_provider_id, provider_type, std::move(client_request),
        std::move(host_ptr_info), default_loader_factory_getter);
  }
}

// Constructor for service worker execution contexts.
ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider(
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr info) {
  // Initialize the provider context with info for
  // ServiceWorkerGlobalScope#registration.
  ThreadSafeSender* sender = ChildThreadImpl::current()->thread_safe_sender();
  ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
      sender, base::ThreadTaskRunnerHandle::Get().get());
  // TODO(kinuko): Split ServiceWorkerProviderContext ctor for
  // controller and controllee.
  context_ = base::MakeRefCounted<ServiceWorkerProviderContext>(
      info->provider_id, SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,
      std::move(info->client_request), std::move(info->host_ptr_info),
      nullptr /* loader_factory_getter */);
  context_->SetRegistrationForServiceWorkerGlobalScope(
      std::move(info->registration), sender);

  if (info->script_loader_factory_ptr_info.is_valid()) {
    script_loader_factory_.Bind(
        std::move(info->script_loader_factory_ptr_info));
  }
}

}  // namespace content
