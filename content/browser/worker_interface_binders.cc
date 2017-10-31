// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_interface_binders.h"

#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/shape_detection/public/interfaces/barcodedetection.mojom.h"
#include "services/shape_detection/public/interfaces/constants.mojom.h"
#include "services/shape_detection/public/interfaces/facedetection_provider.mojom.h"
#include "services/shape_detection/public/interfaces/textdetection.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

// A holder for a parameterized BinderRegistry for content-layer interfaces
// exposed to web workers.
class WorkerInterfaceBinders {
 public:
  WorkerInterfaceBinders() { InitializeParameterizedBinderRegistry(); }

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

 private:
  void InitializeParameterizedBinderRegistry();

  service_manager::BinderRegistryWithArgs<RenderProcessHost*,
                                          const url::Origin&>
      parameterized_binder_registry_;
};

// Forwards service requests to Service Manager since the renderer cannot launch
// out-of-process services on is own.
template <typename Interface>
void ForwardRequest(const char* service_name,
                    mojo::InterfaceRequest<Interface> request,
                    RenderProcessHost* host,
                    const url::Origin& origin) {
  auto* connector = BrowserContext::GetConnectorFor(host->GetBrowserContext());
  connector->BindInterface(service_name, std::move(request));
}

void WorkerInterfaceBinders::InitializeParameterizedBinderRegistry() {
  parameterized_binder_registry_.AddInterface(
      base::Bind(&ForwardRequest<shape_detection::mojom::BarcodeDetection>,
                 shape_detection::mojom::kServiceName));
  parameterized_binder_registry_.AddInterface(
      base::Bind(&ForwardRequest<shape_detection::mojom::FaceDetectionProvider>,
                 shape_detection::mojom::kServiceName));
  parameterized_binder_registry_.AddInterface(
      base::Bind(&ForwardRequest<shape_detection::mojom::TextDetection>,
                 shape_detection::mojom::kServiceName));
}

}  // namespace

void BindWorkerInterface(const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle interface_pipe,
                         RenderProcessHost* host,
                         const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CR_DEFINE_STATIC_LOCAL(WorkerInterfaceBinders, binders, ());
  binders.BindInterface(interface_name, std::move(interface_pipe), host,
                        origin);
}

}  // namespace content
