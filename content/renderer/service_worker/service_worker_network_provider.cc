// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_network_provider.h"

#include "base/atomic_sequence_num.h"
#include "content/child/child_thread_impl.h"
#include "content/common/navigation_params.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/web_service_worker_network_provider_base_impl.h"
#include "content/renderer/service_worker/web_service_worker_network_provider_impl_for_frame.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_local_frame.h"

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

}  // namespace

std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
ServiceWorkerNetworkProvider::CreateInvalidInstanceForNavigation() {
  return std::make_unique<WebServiceWorkerNetworkProviderImplForFrame>(
      base::WrapUnique(new ServiceWorkerNetworkProvider()), nullptr);
}

// static
std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
ServiceWorkerNetworkProvider::CreateForNavigation(
    RenderFrameImpl* frame,
    const CommitNavigationParams* commit_params,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory) {
  blink::WebLocalFrame* web_frame = frame->GetWebFrame();
  // Determine if a ServiceWorkerNetworkProvider should be created and properly
  // initialized for the navigation. A default ServiceWorkerNetworkProvider
  // will always be created since it is expected in a certain number of places,
  // however it will have an invalid id.
  bool should_create_provider = false;
  int provider_id = kInvalidServiceWorkerProviderId;
  if (commit_params) {
    should_create_provider = commit_params->should_create_service_worker;
    provider_id = commit_params->service_worker_provider_id;
  } else {
    // It'd be convenient to check web_frame->GetSecurityOrigin().IsOpaque()
    // here instead of just looking at the sandbox flags, but
    // GetSecurityOrigin() crashes because the frame does not yet have a
    // security context.
    should_create_provider =
        ((web_frame->EffectiveSandboxFlags() &
          blink::WebSandboxFlags::kOrigin) != blink::WebSandboxFlags::kOrigin);
  }

  // If we shouldn't create a real ServiceWorkerNetworkProvider, return one with
  // an invalid id.
  if (!should_create_provider) {
    return CreateInvalidInstanceForNavigation();
  }

  // Otherwise, create the ServiceWorkerNetworkProvider.

  // Ideally Document::IsSecureContext would be called here, but the document is
  // not created yet, and due to redirects the URL may change. So pass
  // is_parent_frame_secure to the browser process, so it can determine the
  // context security when deciding whether to allow a service worker to control
  // the document.
  const bool is_parent_frame_secure = IsFrameSecure(web_frame->Parent());

  // If the browser process did not assign a provider id already, assign one
  // now (see class comments for content::ServiceWorkerProviderHost).
  DCHECK(ServiceWorkerUtils::IsBrowserAssignedProviderId(provider_id) ||
         provider_id == kInvalidServiceWorkerProviderId);
  if (provider_id == kInvalidServiceWorkerProviderId)
    provider_id = GetNextProviderId();

  auto provider = base::WrapUnique(new ServiceWorkerNetworkProvider(
      frame->GetRoutingID(),
      blink::mojom::ServiceWorkerProviderType::kForWindow, provider_id,
      is_parent_frame_secure, std::move(controller_info),
      std::move(fallback_loader_factory)));
  return std::make_unique<WebServiceWorkerNetworkProviderImplForFrame>(
      std::move(provider), frame);
}

// static
std::unique_ptr<ServiceWorkerNetworkProvider>
ServiceWorkerNetworkProvider::CreateForSharedWorker(
    blink::mojom::ServiceWorkerProviderInfoForSharedWorkerPtr info,
    network::mojom::URLLoaderFactoryAssociatedPtrInfo
        script_loader_factory_info,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory) {
  // S13nServiceWorker: |info| holds info about the precreated provider host.
  if (info) {
    DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());
    return base::WrapUnique(new ServiceWorkerNetworkProvider(
        std::move(info), std::move(script_loader_factory_info),
        std::move(controller_info), std::move(fallback_loader_factory)));
  }

  return base::WrapUnique(new ServiceWorkerNetworkProvider(
      MSG_ROUTING_NONE,
      blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
      GetNextProviderId(), true /* is_parent_frame_secure */,
      nullptr /* controller_service_worker */,
      std::move(fallback_loader_factory)));
}

// static
ServiceWorkerNetworkProvider*
ServiceWorkerNetworkProvider::FromWebServiceWorkerNetworkProvider(
    blink::WebServiceWorkerNetworkProvider* provider) {
  if (!provider) {
    DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());
    return nullptr;
  }
  return static_cast<WebServiceWorkerNetworkProviderBaseImpl*>(provider)
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

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerNetworkProvider::IsControlledByServiceWorker() const {
  if (!context())
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  return context()->IsControlledByServiceWorker();
}

void ServiceWorkerNetworkProvider::DispatchNetworkQuiet() {
  if (!context())
    return;
  context()->DispatchNetworkQuiet();
}

// Creates an invalid instance (provider_id() returns
// kInvalidServiceWorkerProviderId).
ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider() {}

// Constructor for service worker clients.
ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider(
    int route_id,
    blink::mojom::ServiceWorkerProviderType provider_type,
    int provider_id,
    bool is_parent_frame_secure,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory) {
  DCHECK_NE(provider_id, kInvalidServiceWorkerProviderId);
  DCHECK(provider_type == blink::mojom::ServiceWorkerProviderType::kForWindow ||
         provider_type ==
             blink::mojom::ServiceWorkerProviderType::kForSharedWorker);

  auto host_info = blink::mojom::ServiceWorkerProviderHostInfo::New(
      provider_id, route_id, provider_type, is_parent_frame_secure,
      nullptr /* host_request */, nullptr /* client_ptr_info */);
  blink::mojom::ServiceWorkerContainerAssociatedRequest client_request =
      mojo::MakeRequest(&host_info->client_ptr_info);
  blink::mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  host_info->host_request = mojo::MakeRequest(&host_ptr_info);
  DCHECK(host_info->host_request.is_pending());
  DCHECK(host_info->host_request.handle().is_valid());

  context_ = base::MakeRefCounted<ServiceWorkerProviderContext>(
      provider_id, provider_type, std::move(client_request),
      std::move(host_ptr_info), std::move(controller_info),
      std::move(fallback_loader_factory));

  if (ChildThreadImpl::current()) {
    ChildThreadImpl::current()->channel()->GetRemoteAssociatedInterface(
        &dispatcher_host_);
    dispatcher_host_->OnProviderCreated(std::move(host_info));
  } else {
    // current() may be null in tests. Silently drop messages sent over
    // ServiceWorkerContainerHost since we couldn't set it up correctly due to
    // this test limitation. This way we don't crash when the associated
    // interface ptr is used.
    //
    // TODO(falken): Just give SWPC a null interface ptr and make the callsites
    // deal with it. They are supposed to anyway because
    // OnNetworkProviderDestroyed() can reset the ptr to null at any time.
    mojo::AssociateWithDisconnectedPipe(host_info->host_request.PassHandle());
  }
}

// Constructor for precreated shared worker.
ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider(
    blink::mojom::ServiceWorkerProviderInfoForSharedWorkerPtr info,
    network::mojom::URLLoaderFactoryAssociatedPtrInfo
        script_loader_factory_info,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory) {
  context_ = base::MakeRefCounted<ServiceWorkerProviderContext>(
      info->provider_id,
      blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
      std::move(info->client_request), std::move(info->host_ptr_info),
      std::move(controller_info), std::move(fallback_loader_factory));
  if (script_loader_factory_info.is_valid())
    script_loader_factory_.Bind(std::move(script_loader_factory_info));
}

}  // namespace content
