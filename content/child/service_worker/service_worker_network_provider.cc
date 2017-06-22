// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_network_provider.h"

#include "base/atomic_sequence_num.h"
#include "content/child/child_thread_impl.h"
#include "content/child/request_extra_data.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/common/navigation_params.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_provider_host_info.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerNetworkProvider.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

namespace {

// Must be unique in the child process.
int GetNextProviderId() {
  static base::StaticAtomicSequenceNumber sequence;
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

  bool IsControlledByServiceWorker() override {
    return provider_->IsControlledByServiceWorker();
  }

  int GetProviderID() const override { return provider_->provider_id(); }

  int64_t ServiceWorkerID() override {
    if (provider_->context() && provider_->context()->controller())
      return provider_->context()->controller()->version_id();
    return kInvalidServiceWorkerVersionId;
  }

  ServiceWorkerNetworkProvider* provider() { return provider_.get(); }

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
    bool content_initiated) {
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
      network_provider = std::unique_ptr<ServiceWorkerNetworkProvider>(
          new ServiceWorkerNetworkProvider(route_id,
                                           SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                           is_parent_frame_secure));
    } else {
      CHECK(browser_side_navigation);
      DCHECK(ServiceWorkerUtils::IsBrowserAssignedProviderId(
          service_worker_provider_id));
      network_provider = std::unique_ptr<ServiceWorkerNetworkProvider>(
          new ServiceWorkerNetworkProvider(
              route_id, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
              service_worker_provider_id, is_parent_frame_secure));
    }
  } else {
    network_provider = std::unique_ptr<ServiceWorkerNetworkProvider>(
        new ServiceWorkerNetworkProvider());
  }
  return base::MakeUnique<WebServiceWorkerNetworkProviderForFrame>(
      std::move(network_provider));
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

ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider(
    int route_id,
    ServiceWorkerProviderType provider_type,
    int browser_provider_id,
    bool is_parent_frame_secure)
    : provider_id_(browser_provider_id) {
  if (provider_id_ == kInvalidServiceWorkerProviderId)
    return;
  if (!ChildThreadImpl::current())
    return;  // May be null in some tests.

  ServiceWorkerProviderHostInfo host_info(provider_id_, route_id, provider_type,
                                          is_parent_frame_secure);
  host_info.host_request = mojo::MakeRequest(&provider_host_);
  mojom::ServiceWorkerProviderAssociatedRequest client_request =
      mojo::MakeRequest(&host_info.client_ptr_info);

  DCHECK(host_info.host_request.is_pending());
  DCHECK(host_info.host_request.handle().is_valid());
  context_ = new ServiceWorkerProviderContext(
      provider_id_, provider_type, std::move(client_request),
      ChildThreadImpl::current()->thread_safe_sender());
  ChildThreadImpl::current()->channel()->GetRemoteAssociatedInterface(
      &dispatcher_host_);
  dispatcher_host_->OnProviderCreated(std::move(host_info));
}

ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider(
    int route_id,
    ServiceWorkerProviderType provider_type,
    bool is_parent_frame_secure)
    : ServiceWorkerNetworkProvider(route_id,
                                   provider_type,
                                   GetNextProviderId(),
                                   is_parent_frame_secure) {}

ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider(
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr info)
    : provider_id_(info->provider_id) {
  context_ = new ServiceWorkerProviderContext(
      provider_id_, SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,
      std::move(info->client_request),
      ChildThreadImpl::current()->thread_safe_sender());

  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
          ChildThreadImpl::current()->thread_safe_sender(),
          base::ThreadTaskRunnerHandle::Get().get());
  // TODO(shimazu): Set registration/attributes directly to |context_|.
  dispatcher->OnAssociateRegistration(-1 /* unused thread_id */,
                                      info->provider_id, info->registration,
                                      info->attributes);
  provider_host_.Bind(std::move(info->host_ptr_info));
}

ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider()
    : provider_id_(kInvalidServiceWorkerProviderId) {}

ServiceWorkerNetworkProvider::~ServiceWorkerNetworkProvider() {
  if (provider_id_ == kInvalidServiceWorkerProviderId)
    return;
  if (!ChildThreadImpl::current())
    return;  // May be null in some tests.
  provider_host_.reset();
}

bool ServiceWorkerNetworkProvider::IsControlledByServiceWorker() const {
  if (ServiceWorkerUtils::IsServicificationEnabled()) {
    // Interception for subresource loading is not working (yet)
    // when servicification is enabled.
    return false;
  }
  return context() && context()->controller();
}

}  // namespace content
