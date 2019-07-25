// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_provider_context.h"

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_subresource_loader.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "content/renderer/worker/worker_thread_registry.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

void CreateSubresourceLoaderFactoryForProviderContext(
    blink::mojom::ServiceWorkerContainerHostPtrInfo container_host_info,
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
        remote_controller,
    const std::string& client_id,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> fallback_factory_info,
    blink::mojom::ControllerServiceWorkerConnectorRequest connector_request,
    network::mojom::URLLoaderFactoryRequest request,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  auto connector = base::MakeRefCounted<ControllerServiceWorkerConnector>(
      std::move(container_host_info), std::move(remote_controller), client_id);
  connector->AddBinding(std::move(connector_request));
  ServiceWorkerSubresourceLoaderFactory::Create(
      std::move(connector),
      network::SharedURLLoaderFactory::Create(std::move(fallback_factory_info)),
      std::move(request), std::move(task_runner));
}

}  // namespace

// For service worker clients.
ServiceWorkerProviderContext::ServiceWorkerProviderContext(
    blink::mojom::ServiceWorkerProviderType provider_type,
    blink::mojom::ServiceWorkerContainerAssociatedRequest request,
    blink::mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory)
    : provider_type_(provider_type),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this, std::move(request)),
      fallback_loader_factory_(std::move(fallback_loader_factory)) {
  container_host_.Bind(std::move(host_ptr_info));

  // Set up the URL loader factory for sending subresource requests to
  // the controller.
  if (controller_info) {
    SetController(std::move(controller_info),
                  false /* should_notify_controllerchange */);
  }
}

ServiceWorkerProviderContext::~ServiceWorkerProviderContext() {
  if (weak_wrapped_subresource_loader_factory_)
    weak_wrapped_subresource_loader_factory_->Detach();
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerProviderContext::TakeController() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return std::move(controller_);
}

int64_t ServiceWorkerProviderContext::GetControllerVersionId() const {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return controller_version_id_;
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerProviderContext::GetControllerServiceWorkerMode() const {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return controller_mode_;
}

network::mojom::URLLoaderFactory*
ServiceWorkerProviderContext::GetSubresourceLoaderFactoryInternal() {
  if (!remote_controller_ && !controller_connector_) {
    // No controller is attached.
    return nullptr;
  }

  if (controller_mode_ !=
      blink::mojom::ControllerServiceWorkerMode::kControlled) {
    // The controller does not exist or has no fetch event handler.
    return nullptr;
  }

  if (!subresource_loader_factory_) {
    DCHECK(!controller_connector_);
    DCHECK(remote_controller_);

    blink::mojom::ServiceWorkerContainerHostPtrInfo host_ptr_info =
        CloneContainerHostPtrInfo();
    if (!host_ptr_info)
      return nullptr;

    // Create a SubresourceLoaderFactory on a background thread to avoid
    // extra contention on the main thread.
    auto task_runner = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&CreateSubresourceLoaderFactoryForProviderContext,
                       std::move(host_ptr_info), std::move(remote_controller_),
                       client_id_, fallback_loader_factory_->Clone(),
                       mojo::MakeRequest(&controller_connector_),
                       mojo::MakeRequest(&subresource_loader_factory_),
                       task_runner));

    DCHECK(!weak_wrapped_subresource_loader_factory_);
    weak_wrapped_subresource_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            subresource_loader_factory_.get());
  }
  return subresource_loader_factory_.get();
}

scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
ServiceWorkerProviderContext::GetSubresourceLoaderFactory() {
  // If we can't get our internal factory it means the state is not currently
  // good to process new requests regardless of the presence of an existing
  // weak_wrapped_subresource_loader_factory.
  if (!GetSubresourceLoaderFactoryInternal()) {
    return nullptr;
  }

  return weak_wrapped_subresource_loader_factory_;
}

blink::mojom::ServiceWorkerContainerHost*
ServiceWorkerProviderContext::container_host() const {
  DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForWindow,
            provider_type_);
  return container_host_.get();
}

const std::set<blink::mojom::WebFeature>&
ServiceWorkerProviderContext::used_features() const {
  return used_features_;
}

const std::string& ServiceWorkerProviderContext::client_id() const {
  return client_id_;
}

const base::UnguessableToken&
ServiceWorkerProviderContext::fetch_request_window_id() const {
  return fetch_request_window_id_;
}

void ServiceWorkerProviderContext::SetWebServiceWorkerProvider(
    base::WeakPtr<WebServiceWorkerProviderImpl> provider) {
  web_service_worker_provider_ = std::move(provider);
}

void ServiceWorkerProviderContext::RegisterWorkerClient(
    blink::mojom::ServiceWorkerWorkerClientPtr client) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  client.set_connection_error_handler(base::BindOnce(
      &ServiceWorkerProviderContext::UnregisterWorkerFetchContext,
      base::Unretained(this), client.get()));
  worker_clients_.push_back(std::move(client));
}

void ServiceWorkerProviderContext::CloneWorkerClientRegistry(
    blink::mojom::ServiceWorkerWorkerClientRegistryRequest request) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  worker_client_registry_bindings_.AddBinding(this, std::move(request));
}

blink::mojom::ServiceWorkerContainerHostPtrInfo
ServiceWorkerProviderContext::CloneContainerHostPtrInfo() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  if (!container_host_)
    return nullptr;
  blink::mojom::ServiceWorkerContainerHostPtrInfo container_host_ptr_info;
  container_host_->CloneContainerHost(
      mojo::MakeRequest(&container_host_ptr_info));
  return container_host_ptr_info;
}

void ServiceWorkerProviderContext::OnNetworkProviderDestroyed() {
  container_host_.reset();
}

void ServiceWorkerProviderContext::PingContainerHost(
    base::OnceClosure callback) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  if (!container_host_)
    return;
  container_host_->Ping(std::move(callback));
}

void ServiceWorkerProviderContext::DispatchNetworkQuiet() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (controller_mode_ ==
      blink::mojom::ControllerServiceWorkerMode::kNoController) {
    return;
  }

  if (!container_host_)
    return;

  container_host_->HintToUpdateServiceWorker();
}

void ServiceWorkerProviderContext::NotifyExecutionReady() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(provider_type(),
            blink::mojom::ServiceWorkerProviderType::kForWindow)
      << "only windows need to send this message; shared workers have "
         "execution ready set on the browser-side when the response is "
         "committed";
  if (!container_host_)
    return;
  if (sent_execution_ready_) {
    // Sometimes a new document can be created for a frame without a proper
    // navigation, in cases like about:blank and javascript: URLs. In these
    // cases the provider is not recreated and Blink can tell us that it's
    // execution ready more than once. The browser-side host doesn't support
    // changing the URL of the provider in these cases, so just ignore these
    // notifications.
    return;
  }
  sent_execution_ready_ = true;
  container_host_->OnExecutionReady();
}

void ServiceWorkerProviderContext::UnregisterWorkerFetchContext(
    blink::mojom::ServiceWorkerWorkerClient* client) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  base::EraseIf(
      worker_clients_,
      [client](const blink::mojom::ServiceWorkerWorkerClientPtr& client_ptr) {
        return client_ptr.get() == client;
      });
}

void ServiceWorkerProviderContext::SetController(
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    bool should_notify_controllerchange) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  controller_ = std::move(controller_info->object_info);
  controller_version_id_ = controller_
                               ? controller_->version_id
                               : blink::mojom::kInvalidServiceWorkerVersionId;
  // The client id should never change once set.
  DCHECK(client_id_.empty() || client_id_ == controller_info->client_id);
  client_id_ = controller_info->client_id;

  if (controller_info->fetch_request_window_id) {
    DCHECK(controller_);
    fetch_request_window_id_ = *controller_info->fetch_request_window_id;
  } else {
    fetch_request_window_id_ = base::UnguessableToken();
  }

  DCHECK((controller_info->mode ==
              blink::mojom::ControllerServiceWorkerMode::kNoController &&
          !controller_) ||
         (controller_info->mode !=
              blink::mojom::ControllerServiceWorkerMode::kNoController &&
          controller_));
  controller_mode_ = controller_info->mode;
  remote_controller_ = std::move(controller_info->remote_controller);

  // Propagate the controller to workers related to this provider.
  if (controller_) {
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              controller_->version_id);
    for (const auto& worker : worker_clients_) {
      // This is a Mojo interface call to the (dedicated or shared) worker
      // thread.
      worker->OnControllerChanged(controller_mode_);
    }
  }
  for (blink::mojom::WebFeature feature : controller_info->used_features)
    used_features_.insert(feature);

  // Reset connector state for subresource loader factory if necessary.
  if (CanCreateSubresourceLoaderFactory()) {
    // There could be four patterns:
    //  (A) Had a controller, and got a new controller.
    //  (B) Had a controller, and lost the controller.
    //  (C) Didn't have a controller, and got a new controller.
    //  (D) Didn't have a controller, and lost the controller (nothing to do).
    if (controller_connector_) {
      // Used to have a controller at least once and have created a
      // subresource loader factory before (if no subresource factory was
      // created before, then the right controller, if any, will be used when
      // the factory is created in GetSubresourceLoaderFactory, so there's
      // nothing to do here).
      // Update the connector's controller so that subsequent resource requests
      // will get the new controller in case (A)/(C), or fallback to the network
      // in case (B). Inflight requests that are already dispatched may just use
      // the existing controller or may use the new controller settings
      // depending on when the request is actually passed to the factory (this
      // part is inherently racy).
      controller_connector_->UpdateController(std::move(remote_controller_));
    }
  }

  // The WebServiceWorkerProviderImpl might not exist yet because the document
  // has not yet been created (as WebServiceWorkerProviderImpl is created for a
  // ServiceWorkerContainer). In that case, once it's created it will still get
  // the controller from |this| via WebServiceWorkerProviderImpl::SetClient().
  if (web_service_worker_provider_) {
    web_service_worker_provider_->SetController(
        std::move(controller_), used_features_, should_notify_controllerchange);
  }
}

void ServiceWorkerProviderContext::PostMessageToClient(
    blink::mojom::ServiceWorkerObjectInfoPtr source,
    blink::TransferableMessage message) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (web_service_worker_provider_) {
    web_service_worker_provider_->PostMessageToClient(std::move(source),
                                                      std::move(message));
  }
}

void ServiceWorkerProviderContext::CountFeature(
    blink::mojom::WebFeature feature) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  // ServiceWorkerProviderContext keeps track of features in order to propagate
  // it to WebServiceWorkerProviderClient, which actually records the
  // UseCounter.
  used_features_.insert(feature);
  if (web_service_worker_provider_) {
    web_service_worker_provider_->CountFeature(feature);
  }
}

bool ServiceWorkerProviderContext::CanCreateSubresourceLoaderFactory() const {
  // |fallback_loader_factory| could be null in unit tests.
  return fallback_loader_factory_ != nullptr;
}

void ServiceWorkerProviderContext::DestructOnMainThread() const {
  if (!main_thread_task_runner_->RunsTasksInCurrentSequence() &&
      main_thread_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

}  // namespace content
