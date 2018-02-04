// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "content/browser/dedicated_worker/dedicated_worker_host.h"

#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

// A host for a single dedicated worker. Its lifetime is managed by the
// DedicatedWorkerGlobalScope of the corresponding worker in the renderer via a
// StrongBinding.
class DedicatedWorkerHost : public service_manager::mojom::InterfaceProvider {
 public:
  DedicatedWorkerHost(int process_id, const url::Origin& origin)
      : process_id_(process_id), origin_(origin) {}

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override {
    RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
    if (!process)
      return;

    BindWorkerInterface(interface_name, std::move(interface_pipe), process,
                        origin_);
  }

 private:
  const int process_id_;
  const url::Origin origin_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHost);
};

// A factory for creating DedicatedWorkerHosts. Its lifetime is managed by
// the renderer over mojo via a StrongBinding.
class DedicatedWorkerFactoryImpl : public blink::mojom::DedicatedWorkerFactory {
 public:
  DedicatedWorkerFactoryImpl(int process_id,
                             const url::Origin& parent_context_origin)
      : process_id_(process_id),
        parent_context_origin_(parent_context_origin) {}

  // blink::mojom::DedicatedWorkerFactory:
  void CreateDedicatedWorker(
      const url::Origin& origin,
      service_manager::mojom::InterfaceProviderRequest request) override {
    // TODO(crbug.com/729021): Once |parent_context_origin_| is no longer races
    // with the request for |DedicatedWorkerFactory|, enforce that the worker's
    // origin either matches the creating document's origin, or is unique.
    mojo::MakeStrongBinding(
        std::make_unique<DedicatedWorkerHost>(process_id_, origin),
        FilterRendererExposedInterfaces(
            blink::mojom::kNavigation_DedicatedWorkerSpec, process_id_,
            std::move(request)));
  }

 private:
  const int process_id_;
  const url::Origin parent_context_origin_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerFactoryImpl);
};

}  // namespace

void CreateDedicatedWorkerHostFactory(
    blink::mojom::DedicatedWorkerFactoryRequest request,
    RenderProcessHost* host,
    const url::Origin& origin) {
  mojo::MakeStrongBinding(
      std::make_unique<DedicatedWorkerFactoryImpl>(host->GetID(), origin),
      std::move(request));
}

}  // namespace content
