// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_provider_context.h"

#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_thread_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_subresource_loader.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "content/renderer/service_worker/web_service_worker_registration_impl.h"
#include "content/renderer/worker_thread_registry.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

// Holds state for service worker clients.
struct ServiceWorkerProviderContext::ProviderStateForClient {
  explicit ProviderStateForClient(
      scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory)
      : default_loader_factory(std::move(default_loader_factory)) {}
  ~ProviderStateForClient() = default;

  // |controller| will be set by SetController() and taken by TakeController().
  blink::mojom::ServiceWorkerObjectInfoPtr controller;
  // Keeps version id of the current controller service worker object.
  int64_t controller_version_id = blink::mojom::kInvalidServiceWorkerVersionId;

  // S13nServiceWorker:
  // Used to intercept requests from the controllee and dispatch them
  // as events to the controller ServiceWorker.
  network::mojom::URLLoaderFactoryPtr subresource_loader_factory;

  // S13nServiceWorker:
  // Used when we create |subresource_loader_factory|.
  scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory;

  // Tracks feature usage for UseCounter.
  std::set<blink::mojom::WebFeature> used_features;

  // Corresponds to a ServiceWorkerContainer. We notify it when
  // ServiceWorkerContainer#controller should be changed.
  base::WeakPtr<WebServiceWorkerProviderImpl> web_service_worker_provider;

  // Keeps ServiceWorkerWorkerClient pointers of dedicated or shared workers
  // which are associated with the ServiceWorkerProviderContext.
  // - If this ServiceWorkerProviderContext is for a Document, then
  //   |worker_clients| contains all its dedicated workers.
  // - If this ServiceWorkerProviderContext is for a SharedWorker (technically
  //   speaking, for its shadow page), then |worker_clients| has one element:
  //   the shared worker.
  std::vector<mojom::ServiceWorkerWorkerClientPtr> worker_clients;

  // S13nServiceWorker
  // Used in |subresource_loader_factory| to get the connection to the
  // controller service worker. Kept here in order to call
  // OnContainerHostConnectionClosed when container_host_ for the
  // provider is reset.
  // This is (re)set to nullptr if no controller is attached to this client.
  scoped_refptr<ControllerServiceWorkerConnector> controller_connector;

  // For service worker clients. Map from registration id to JavaScript
  // ServiceWorkerRegistration object.
  std::map<int64_t, WebServiceWorkerRegistrationImpl*> registrations_;
};

// Holds state for service worker execution contexts.
struct ServiceWorkerProviderContext::ProviderStateForServiceWorker {
  ProviderStateForServiceWorker() = default;
  ~ProviderStateForServiceWorker() = default;
  // |registration| is set by SetRegistrationForServiceWorkerGlobalScope() and
  // taken by TakeRegistrationForServiceWorkerGlobalScope().
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration;
};

// For service worker clients.
ServiceWorkerProviderContext::ServiceWorkerProviderContext(
    int provider_id,
    blink::mojom::ServiceWorkerProviderType provider_type,
    mojom::ServiceWorkerContainerAssociatedRequest request,
    mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info,
    mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory)
    : provider_type_(provider_type),
      provider_id_(provider_id),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  container_host_.Bind(std::move(host_ptr_info));
  state_for_client_ = std::make_unique<ProviderStateForClient>(
      std::move(default_loader_factory));

  if (!CanCreateSubresourceLoaderFactory() &&
      !IsNavigationMojoResponseEnabled()) {
    return;
  }

  // S13nServiceWorker/NavigationMojoResponse:
  // Set up the URL loader factory for sending subresource requests to
  // the controller.
  DCHECK(ServiceWorkerUtils::IsServicificationEnabled() ||
         IsNavigationMojoResponseEnabled());
  if (controller_info) {
    SetController(std::move(controller_info),
                  std::vector<blink::mojom::WebFeature>(),
                  false /* should_notify_controllerchange */);
  }
}

// For service worker execution contexts.
ServiceWorkerProviderContext::ServiceWorkerProviderContext(
    int provider_id,
    mojom::ServiceWorkerContainerAssociatedRequest request,
    mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info)
    : provider_type_(
          blink::mojom::ServiceWorkerProviderType::kForServiceWorker),
      provider_id_(provider_id),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  state_for_service_worker_ = std::make_unique<ProviderStateForServiceWorker>();

  container_host_.Bind(std::move(host_ptr_info));
}

ServiceWorkerProviderContext::~ServiceWorkerProviderContext() = default;

void ServiceWorkerProviderContext::SetRegistrationForServiceWorkerGlobalScope(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  ProviderStateForServiceWorker* state = state_for_service_worker_.get();
  DCHECK(state);
  DCHECK(!state->registration);

  state->registration = std::move(registration);
}

scoped_refptr<WebServiceWorkerRegistrationImpl>
ServiceWorkerProviderContext::TakeRegistrationForServiceWorkerGlobalScope(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
            provider_type_);
  ProviderStateForServiceWorker* state = state_for_service_worker_.get();
  DCHECK(state);
  DCHECK(state->registration);
  DCHECK(state->registration->host_ptr_info.is_valid());
  DCHECK_NE(state->registration->registration_id,
            blink::mojom::kInvalidServiceWorkerRegistrationId);

  DCHECK(state->registration->request.is_pending());
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration =
      WebServiceWorkerRegistrationImpl::CreateForServiceWorkerGlobalScope(
          std::move(state->registration), std::move(io_task_runner));
  return registration;
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerProviderContext::TakeController() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_for_client_);
  return std::move(state_for_client_->controller);
}

int64_t ServiceWorkerProviderContext::GetControllerVersionId() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_for_client_);
  return state_for_client_->controller_version_id;
}

network::mojom::URLLoaderFactory*
ServiceWorkerProviderContext::GetSubresourceLoaderFactory() {
  DCHECK(state_for_client_);
  auto* state = state_for_client_.get();
  if (!state->controller_connector ||
      state->controller_connector->state() ==
          ControllerServiceWorkerConnector::State::kNoController) {
    // No controller is attached.
    return nullptr;
  }
  DCHECK(ServiceWorkerUtils::IsServicificationEnabled());
  if (!state->subresource_loader_factory) {
    ServiceWorkerSubresourceLoaderFactory::Create(
        state->controller_connector, state->default_loader_factory,
        mojo::MakeRequest(&state->subresource_loader_factory));
  }
  return state->subresource_loader_factory.get();
}

mojom::ServiceWorkerContainerHost*
ServiceWorkerProviderContext::container_host() const {
  DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForWindow,
            provider_type_);
  return container_host_.get();
}

const std::set<blink::mojom::WebFeature>&
ServiceWorkerProviderContext::used_features() const {
  DCHECK(state_for_client_);
  return state_for_client_->used_features;
}

void ServiceWorkerProviderContext::SetWebServiceWorkerProvider(
    base::WeakPtr<WebServiceWorkerProviderImpl> provider) {
  DCHECK(state_for_client_);
  state_for_client_->web_service_worker_provider = provider;
}

mojom::ServiceWorkerWorkerClientRequest
ServiceWorkerProviderContext::CreateWorkerClientRequest() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_for_client_);
  mojom::ServiceWorkerWorkerClientPtr client;
  mojom::ServiceWorkerWorkerClientRequest request = mojo::MakeRequest(&client);
  client.set_connection_error_handler(base::BindOnce(
      &ServiceWorkerProviderContext::UnregisterWorkerFetchContext,
      base::Unretained(this), client.get()));
  state_for_client_->worker_clients.push_back(std::move(client));
  return request;
}

mojom::ServiceWorkerContainerHostPtrInfo
ServiceWorkerProviderContext::CloneContainerHostPtrInfo() {
  DCHECK(ServiceWorkerUtils::IsServicificationEnabled());
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_for_client_);
  mojom::ServiceWorkerContainerHostPtrInfo container_host_ptr_info;
  container_host_->CloneForWorker(mojo::MakeRequest(&container_host_ptr_info));
  return container_host_ptr_info;
}

scoped_refptr<WebServiceWorkerRegistrationImpl>
ServiceWorkerProviderContext::GetOrCreateRegistrationForServiceWorkerClient(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info) {
  DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForWindow,
            provider_type_);
  DCHECK(state_for_client_);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);

  auto found = state_for_client_->registrations_.find(info->registration_id);
  if (found != state_for_client_->registrations_.end()) {
    DCHECK(!info->request.is_pending());
    found->second->AttachForServiceWorkerClient(std::move(info));
    return found->second;
  }

  DCHECK(info->request.is_pending());
  // WebServiceWorkerRegistrationImpl constructor calls
  // AddServiceWorkerRegistration to add itself into
  // |state_for_client_->registrations_|.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration =
      WebServiceWorkerRegistrationImpl::CreateForServiceWorkerClient(
          std::move(info), weak_factory_.GetWeakPtr());
  return registration;
}

void ServiceWorkerProviderContext::OnNetworkProviderDestroyed() {
  container_host_.reset();
  if (state_for_client_ && state_for_client_->controller_connector)
    state_for_client_->controller_connector->OnContainerHostConnectionClosed();
}

void ServiceWorkerProviderContext::UnregisterWorkerFetchContext(
    mojom::ServiceWorkerWorkerClient* client) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_for_client_);
  base::EraseIf(
      state_for_client_->worker_clients,
      [client](const mojom::ServiceWorkerWorkerClientPtr& client_ptr) {
        return client_ptr.get() == client;
      });
}

void ServiceWorkerProviderContext::SetController(
    mojom::ControllerServiceWorkerInfoPtr controller_info,
    const std::vector<blink::mojom::WebFeature>& used_features,
    bool should_notify_controllerchange) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  ProviderStateForClient* state = state_for_client_.get();
  DCHECK(state);

  state->controller = std::move(controller_info->object_info);
  state->controller_version_id =
      state->controller ? state->controller->version_id
                        : blink::mojom::kInvalidServiceWorkerVersionId;

  // Propagate the controller to workers related to this provider.
  if (state->controller) {
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerHandleId,
              state->controller->handle_id);
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              state->controller->version_id);
    for (const auto& worker : state->worker_clients) {
      // This is a Mojo interface call to the (dedicated or shared) worker
      // thread.
      worker->SetControllerServiceWorker(state->controller->version_id);
    }
  }
  for (blink::mojom::WebFeature feature : used_features)
    state->used_features.insert(feature);

  // S13nServiceWorker:
  // Reset subresource loader factory if necessary.
  if (CanCreateSubresourceLoaderFactory()) {
    DCHECK(ServiceWorkerUtils::IsServicificationEnabled());

    // There could be four patterns:
    //  (A) Had a controller, and got a new controller.
    //  (B) Had a controller, and lost the controller.
    //  (C) Didn't have a controller, and got a new controller.
    //  (D) Didn't have a controller, and lost the controller (nothing to do).
    if (state->controller_connector) {
      // Used to have a controller at least once.
      // Reset the existing connector so that subsequent resource requests
      // will get the new controller in case (A)/(C), or fallback to the
      // network in case (B). Inflight requests that are already dispatched may
      // just use the existing controller or may use the new controller
      // settings depending on when the request is actually passed to the
      // factory (this part is inherently racy).
      state->controller_connector->ResetControllerConnection(
          mojom::ControllerServiceWorkerPtr(
              std::move(controller_info->endpoint)),
          controller_info->client_id);
    } else if (state->controller) {
      // Case (C): never had a controller, but got a new one now.
      // Set a new |state->controller_connector| so that subsequent resource
      // requests will see it.
      mojom::ControllerServiceWorkerPtr controller_ptr(
          std::move(controller_info->endpoint));
      state->controller_connector =
          base::MakeRefCounted<ControllerServiceWorkerConnector>(
              container_host_.get(), std::move(controller_ptr),
              controller_info->client_id);
    }
  }

  // The WebServiceWorkerProviderImpl might not exist yet because the document
  // has not yet been created (as WebServiceWorkerImpl is created for a
  // ServiceWorkerContainer). In that case, once it's created it will still get
  // the controller from |this| via WebServiceWorkerProviderImpl::SetClient().
  if (state->web_service_worker_provider) {
    state->web_service_worker_provider->SetController(
        std::move(state->controller), state->used_features,
        should_notify_controllerchange);
  }
}

void ServiceWorkerProviderContext::PostMessageToClient(
    blink::mojom::ServiceWorkerObjectInfoPtr source,
    blink::TransferableMessage message) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  ProviderStateForClient* state = state_for_client_.get();
  DCHECK(state);
  if (state->web_service_worker_provider) {
    state->web_service_worker_provider->PostMessageToClient(std::move(source),
                                                            std::move(message));
  }
}

void ServiceWorkerProviderContext::AddServiceWorkerRegistration(
    int64_t registration_id,
    WebServiceWorkerRegistrationImpl* registration) {
  DCHECK(state_for_client_);
  DCHECK(
      !base::ContainsKey(state_for_client_->registrations_, registration_id));
  state_for_client_->registrations_[registration_id] = registration;
}

void ServiceWorkerProviderContext::RemoveServiceWorkerRegistration(
    int64_t registration_id) {
  DCHECK(state_for_client_);
  DCHECK(base::ContainsKey(state_for_client_->registrations_, registration_id));
  state_for_client_->registrations_.erase(registration_id);
}

bool ServiceWorkerProviderContext::ContainsServiceWorkerRegistrationForTesting(
    int64_t registration_id) {
  DCHECK(state_for_client_);
  return base::ContainsKey(state_for_client_->registrations_, registration_id);
}

void ServiceWorkerProviderContext::CountFeature(
    blink::mojom::WebFeature feature) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_for_client_);
  ProviderStateForClient* state = state_for_client_.get();

  // ServiceWorkerProviderContext keeps track of features in order to propagate
  // it to WebServiceWorkerProviderClient, which actually records the
  // UseCounter.
  state->used_features.insert(feature);
  if (state->web_service_worker_provider) {
    state->web_service_worker_provider->CountFeature(feature);
  }
}

bool ServiceWorkerProviderContext::CanCreateSubresourceLoaderFactory() const {
  // Expected that it is called only for clients.
  DCHECK(state_for_client_);
  // |state_for_client_->default_loader_factory| could be null
  // for SharedWorker case (which is not supported by S13nServiceWorker
  // yet, https://crbug.com/796819) and in unit tests, return early in such
  // cases too.
  return (ServiceWorkerUtils::IsServicificationEnabled() &&
          state_for_client_->default_loader_factory);
}

void ServiceWorkerProviderContext::DestructOnMainThread() const {
  if (!main_thread_task_runner_->RunsTasksInCurrentSequence() &&
      main_thread_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

}  // namespace content
