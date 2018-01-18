// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_interface_binders.h"

#include <utility>

#include "base/bind.h"
#include "content/browser/background_fetch/background_fetch_service_impl.h"
#include "content/browser/dedicated_worker/dedicated_worker_host.h"
#include "content/browser/locks/lock_manager.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/payments/payment_manager.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/websockets/websocket_manager.h"
#include "content/network/restricted_cookie_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/device/public/interfaces/vibration_manager.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/shape_detection/public/interfaces/barcodedetection.mojom.h"
#include "services/shape_detection/public/interfaces/constants.mojom.h"
#include "services/shape_detection/public/interfaces/facedetection_provider.mojom.h"
#include "services/shape_detection/public/interfaces/textdetection.mojom.h"
#include "third_party/WebKit/public/platform/modules/notifications/notification_service.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

// A holder for a parameterized BinderRegistry for content-layer interfaces
// exposed to web workers.
class RendererInterfaceBinders {
 public:
  RendererInterfaceBinders() { InitializeParameterizedBinderRegistry(); }

  // Bind an interface request |interface_pipe| for |interface_name| received
  // from a web worker with origin |origin| hosted in the renderer |host|.
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     RenderProcessHost* host,
                     const url::Origin& origin) {
    if (parameterized_binder_registry_.TryBindInterface(
            interface_name, &interface_pipe, host, origin)) {
      return;
    }

    GetContentClient()->browser()->BindInterfaceRequestFromWorker(
        host, origin, interface_name, std::move(interface_pipe));
  }

  // Try binding an interface request |interface_pipe| for |interface_name|
  // received from |frame|.
  bool TryBindInterface(const std::string& interface_name,
                        mojo::ScopedMessagePipeHandle* interface_pipe,
                        RenderFrameHost* frame) {
    return parameterized_binder_registry_.TryBindInterface(
        interface_name, interface_pipe, frame->GetProcess(),
        frame->GetLastCommittedOrigin());
  }

 private:
  void InitializeParameterizedBinderRegistry();

  service_manager::BinderRegistryWithArgs<RenderProcessHost*,
                                          const url::Origin&>
      parameterized_binder_registry_;
};

// Forwards service requests to Service Manager since the renderer cannot launch
// out-of-process services on is own.
template <typename Interface>
void ForwardServiceRequest(const char* service_name,
                           mojo::InterfaceRequest<Interface> request,
                           RenderProcessHost* host,
                           const url::Origin& origin) {
  auto* connector = BrowserContext::GetConnectorFor(host->GetBrowserContext());
  connector->BindInterface(service_name, std::move(request));
}

void GetRestrictedCookieManagerForWorker(
    network::mojom::RestrictedCookieManagerRequest request,
    RenderProcessHost* render_process_host,
    const url::Origin& origin) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    return;
  }

  StoragePartition* storage_partition =
      render_process_host->GetStoragePartition();
  network::mojom::NetworkContext* network_context =
      storage_partition->GetNetworkContext();
  uint32_t render_process_id = render_process_host->GetID();
  network_context->GetRestrictedCookieManager(
      std::move(request), render_process_id, MSG_ROUTING_NONE);
}

// Register renderer-exposed interfaces. Each registered interface binder is
// exposed to all renderer-hosted execution context types (document/frame,
// dedicated worker, shared worker and service worker) where the appropriate
// capability spec in the content_browser manifest includes the interface. For
// interface requests from frames, binders registered on the frame itself
// override binders registered here.
void RendererInterfaceBinders::InitializeParameterizedBinderRegistry() {
  parameterized_binder_registry_.AddInterface(base::Bind(
      &ForwardServiceRequest<shape_detection::mojom::BarcodeDetection>,
      shape_detection::mojom::kServiceName));
  parameterized_binder_registry_.AddInterface(base::Bind(
      &ForwardServiceRequest<shape_detection::mojom::FaceDetectionProvider>,
      shape_detection::mojom::kServiceName));
  parameterized_binder_registry_.AddInterface(
      base::Bind(&ForwardServiceRequest<shape_detection::mojom::TextDetection>,
                 shape_detection::mojom::kServiceName));
  parameterized_binder_registry_.AddInterface(
      base::Bind(&ForwardServiceRequest<device::mojom::VibrationManager>,
                 device::mojom::kServiceName));
  parameterized_binder_registry_.AddInterface(
      base::Bind([](blink::mojom::WebSocketRequest request,
                    RenderProcessHost* host, const url::Origin& origin) {
        WebSocketManager::CreateWebSocket(host->GetID(), MSG_ROUTING_NONE,
                                          std::move(request));
      }));
  parameterized_binder_registry_.AddInterface(
      base::Bind([](payments::mojom::PaymentManagerRequest request,
                    RenderProcessHost* host, const url::Origin& origin) {
        static_cast<StoragePartitionImpl*>(host->GetStoragePartition())
            ->GetPaymentAppContext()
            ->CreatePaymentManager(std::move(request));
      }));
  parameterized_binder_registry_.AddInterface(
      base::Bind([](blink::mojom::PermissionServiceRequest request,
                    RenderProcessHost* host, const url::Origin& origin) {
        static_cast<RenderProcessHostImpl*>(host)
            ->permission_service_context()
            .CreateServiceForWorker(std::move(request), origin);
      }));
  parameterized_binder_registry_.AddInterface(base::BindRepeating(
      [](blink::mojom::LockManagerRequest request, RenderProcessHost* host,
         const url::Origin& origin) {
        static_cast<StoragePartitionImpl*>(host->GetStoragePartition())
            ->GetLockManager()
            ->CreateService(std::move(request), origin);
      }));
  parameterized_binder_registry_.AddInterface(
      base::Bind(&CreateDedicatedWorkerHostFactory));
  parameterized_binder_registry_.AddInterface(
      base::Bind([](blink::mojom::NotificationServiceRequest request,
                    RenderProcessHost* host, const url::Origin& origin) {
        static_cast<StoragePartitionImpl*>(host->GetStoragePartition())
            ->GetPlatformNotificationContext()
            ->CreateService(host->GetID(), origin, std::move(request));
      }));
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(&BackgroundFetchServiceImpl::Create));
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(GetRestrictedCookieManagerForWorker));
}

RendererInterfaceBinders& GetRendererInterfaceBinders() {
  CR_DEFINE_STATIC_LOCAL(RendererInterfaceBinders, binders, ());
  return binders;
}

}  // namespace

void BindWorkerInterface(const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle interface_pipe,
                         RenderProcessHost* host,
                         const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetRendererInterfaceBinders().BindInterface(
      interface_name, std::move(interface_pipe), host, origin);
}

bool TryBindFrameInterface(const std::string& interface_name,
                           mojo::ScopedMessagePipeHandle* interface_pipe,
                           RenderFrameHost* frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return GetRendererInterfaceBinders().TryBindInterface(interface_name,
                                                        interface_pipe, frame);
}

}  // namespace content
