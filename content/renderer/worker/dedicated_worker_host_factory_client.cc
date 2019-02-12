// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/dedicated_worker_host_factory_client.h"

#include <utility>
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"
#include "third_party/blink/public/platform/web_dedicated_worker.h"

namespace content {

DedicatedWorkerHostFactoryClient::DedicatedWorkerHostFactoryClient(
    blink::WebDedicatedWorker* worker,
    service_manager::InterfaceProvider* interface_provider)
    : worker_(worker), binding_(this) {
  DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());
  DCHECK(interface_provider);
  interface_provider->GetInterface(mojo::MakeRequest(&factory_));
}

DedicatedWorkerHostFactoryClient::~DedicatedWorkerHostFactoryClient() = default;

void DedicatedWorkerHostFactoryClient::CreateWorkerHost(
    const blink::WebURL& script_url,
    const blink::WebSecurityOrigin& script_origin,
    mojo::ScopedMessagePipeHandle blob_url_token) {
  DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());
  blink::mojom::DedicatedWorkerHostFactoryClientPtr client_ptr;
  binding_.Bind(mojo::MakeRequest(&client_ptr));

  factory_->CreateAndStartLoad(
      script_url, script_origin,
      blink::mojom::BlobURLTokenPtr(blink::mojom::BlobURLTokenPtrInfo(
          std::move(blob_url_token), blink::mojom::BlobURLToken::Version_)),
      std::move(client_ptr));
}

void DedicatedWorkerHostFactoryClient::OnWorkerHostCreated(
    service_manager::mojom::InterfaceProviderPtr interface_provider) {
  worker_->OnWorkerHostCreated(interface_provider.PassInterface().PassHandle());
}

void DedicatedWorkerHostFactoryClient::OnScriptLoaded(
    blink::mojom::ServiceWorkerProviderInfoForWorkerPtr
        service_worker_provider_info,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factory_bundle_info,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info) {
  DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());
  // TODO(nhiroki): Keep the given values for creating
  // WebWorkerFetchContextImpl.
  worker_->OnScriptLoaded();
}

void DedicatedWorkerHostFactoryClient::OnScriptLoadFailed() {
  DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());
  worker_->OnScriptLoadFailed();
  // |this| may be destroyed at this point.
}

}  // namespace content
