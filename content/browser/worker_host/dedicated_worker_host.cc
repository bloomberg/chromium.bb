// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "content/browser/worker_host/dedicated_worker_host.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/websockets/websocket_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

// A host for a single dedicated worker. Its lifetime is managed by the
// DedicatedWorkerGlobalScope of the corresponding worker in the renderer via a
// StrongBinding.
class DedicatedWorkerHost : public service_manager::mojom::InterfaceProvider {
 public:
  DedicatedWorkerHost(int process_id,
                      int ancestor_render_frame_id,
                      const url::Origin& origin)
      : process_id_(process_id),
        ancestor_render_frame_id_(ancestor_render_frame_id),
        origin_(origin) {
    RegisterMojoInterfaces();
  }

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override {
    RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
    if (!process)
      return;

    // See if the registry that is specific to this worker host wants to handle
    // the interface request.
    if (registry_.TryBindInterface(interface_name, &interface_pipe))
      return;

    BindWorkerInterface(interface_name, std::move(interface_pipe), process,
                        origin_);
  }

 private:
  void RegisterMojoInterfaces() {
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateWebSocket, base::Unretained(this)));
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateWebUsbService, base::Unretained(this)));
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateDedicatedWorker, base::Unretained(this)));
  }

  void CreateWebUsbService(blink::mojom::WebUsbServiceRequest request) {
    auto* host =
        RenderFrameHostImpl::FromID(process_id_, ancestor_render_frame_id_);
    GetContentClient()->browser()->CreateWebUsbService(host,
                                                       std::move(request));
  }

  void CreateWebSocket(network::mojom::WebSocketRequest request) {
    network::mojom::AuthenticationHandlerPtr auth_handler;
    auto* frame =
        RenderFrameHost::FromID(process_id_, ancestor_render_frame_id_);
    if (!frame) {
      // In some cases |frame| can be null. In such cases the worker will
      // soon be terminated too, so let's abort the connection.
      request.ResetWithReason(network::mojom::WebSocket::kInsufficientResources,
                              "The parent frame has already been gone.");
      return;
    }

    GetContentClient()->browser()->WillCreateWebSocket(frame, &request,
                                                       &auth_handler);

    WebSocketManager::CreateWebSocket(process_id_, ancestor_render_frame_id_,
                                      origin_, std::move(auth_handler),
                                      std::move(request));
  }

  void CreateDedicatedWorker(
      blink::mojom::DedicatedWorkerFactoryRequest request) {
    CreateDedicatedWorkerHostFactory(process_id_, ancestor_render_frame_id_,
                                     origin_, std::move(request));
  }

  const int process_id_;
  // ancestor_render_frame_id_ is the id of the frame that owns this worker,
  // either directly, or (in the case of nested workers) indirectly via a tree
  // of dedicated workers.
  const int ancestor_render_frame_id_;
  const url::Origin origin_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHost);
};

// A factory for creating DedicatedWorkerHosts. Its lifetime is managed by
// the renderer over mojo via a StrongBinding.
class DedicatedWorkerFactoryImpl : public blink::mojom::DedicatedWorkerFactory {
 public:
  DedicatedWorkerFactoryImpl(int process_id,
                             int ancestor_render_frame_id,
                             const url::Origin& parent_context_origin)
      : process_id_(process_id),
        ancestor_render_frame_id_(ancestor_render_frame_id),
        parent_context_origin_(parent_context_origin) {}

  // blink::mojom::DedicatedWorkerFactory:
  void CreateDedicatedWorker(
      const url::Origin& origin,
      service_manager::mojom::InterfaceProviderRequest request) override {
    // TODO(crbug.com/729021): Once |parent_context_origin_| is no longer races
    // with the request for |DedicatedWorkerFactory|, enforce that the worker's
    // origin either matches the creating document's origin, or is unique.
    mojo::MakeStrongBinding(std::make_unique<DedicatedWorkerHost>(
                                process_id_, ancestor_render_frame_id_, origin),
                            FilterRendererExposedInterfaces(
                                blink::mojom::kNavigation_DedicatedWorkerSpec,
                                process_id_, std::move(request)));
  }

 private:
  const int process_id_;
  const int ancestor_render_frame_id_;
  const url::Origin parent_context_origin_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerFactoryImpl);
};

}  // namespace

void CreateDedicatedWorkerHostFactory(
    int process_id,
    int ancestor_render_frame_id,
    const url::Origin& origin,
    blink::mojom::DedicatedWorkerFactoryRequest request) {
  mojo::MakeStrongBinding(std::make_unique<DedicatedWorkerFactoryImpl>(
                              process_id, ancestor_render_frame_id, origin),
                          std::move(request));
}

}  // namespace content
