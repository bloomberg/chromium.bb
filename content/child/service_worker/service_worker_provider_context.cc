// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_provider_context.h"

#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/controller_service_worker_connector.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_subresource_loader.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_registry.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/child/child_url_loader_factory_getter.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

// Holds state for service worker clients.
struct ServiceWorkerProviderContext::ControlleeState {
  explicit ControlleeState(
      scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter)
      : default_loader_factory_getter(
            std::move(default_loader_factory_getter)) {}
  ~ControlleeState() = default;

  std::unique_ptr<ServiceWorkerHandleReference> controller;

  // S13nServiceWorker:
  // Used to intercept requests from the controllee and dispatch them
  // as events to the controller ServiceWorker. This is reset when a new
  // controller is set.
  mojom::URLLoaderFactoryPtr subresource_loader_factory;

  // S13nServiceWorker:
  // Used when we create |subresource_loader_factory|.
  scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter;

  // Tracks feature usage for UseCounter.
  std::set<uint32_t> used_features;

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
  scoped_refptr<ControllerServiceWorkerConnector> controller_connector;
};

// Holds state for service worker execution contexts.
struct ServiceWorkerProviderContext::ControllerState {
  ControllerState() = default;
  ~ControllerState() = default;
  // |registration->host_ptr_info| will be taken by
  // ServiceWorkerProviderContext::TakeRegistrationForServiceWorkerGlobalScope()
  // means after that |registration| will be in a half-way taken state.
  // TODO(leonhsl): To avoid the half-way taken state mentioned above, make
  // ServiceWorkerProviderContext::TakeRegistrationForServiceWorkerGlobalScope()
  // take/reset all information of |registration|, |installing|, |waiting| and
  // |active| all at once.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration;
  std::unique_ptr<ServiceWorkerHandleReference> installing;
  std::unique_ptr<ServiceWorkerHandleReference> waiting;
  std::unique_ptr<ServiceWorkerHandleReference> active;
};

ServiceWorkerProviderContext::ServiceWorkerProviderContext(
    int provider_id,
    ServiceWorkerProviderType provider_type,
    mojom::ServiceWorkerContainerAssociatedRequest request,
    mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info,
    ServiceWorkerDispatcher* dispatcher,
    scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter)
    : provider_type_(provider_type),
      provider_id_(provider_id),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this, std::move(request)) {
  container_host_.Bind(std::move(host_ptr_info));
  if (provider_type == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER) {
    controller_state_ = base::MakeUnique<ControllerState>();
  } else {
    controllee_state_ = base::MakeUnique<ControlleeState>(
        std::move(default_loader_factory_getter));
  }

  // |dispatcher| may be null in tests.
  // TODO(falken): Figure out how to make a dispatcher in tests.
  if (dispatcher)
    dispatcher->AddProviderContext(this);
}

ServiceWorkerProviderContext::~ServiceWorkerProviderContext() {
  if (ServiceWorkerDispatcher* dispatcher =
          ServiceWorkerDispatcher::GetThreadSpecificInstance()) {
    // Remove this context from the dispatcher living on the main thread.
    dispatcher->RemoveProviderContext(this);
  }
}

void ServiceWorkerProviderContext::SetRegistrationForServiceWorkerGlobalScope(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration,
    std::unique_ptr<ServiceWorkerHandleReference> installing,
    std::unique_ptr<ServiceWorkerHandleReference> waiting,
    std::unique_ptr<ServiceWorkerHandleReference> active) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  ControllerState* state = controller_state_.get();
  DCHECK(state);
  DCHECK(!state->registration);
  DCHECK(!state->installing && !state->waiting && !state->active);
  state->registration = std::move(registration);
  state->installing = std::move(installing);
  state->waiting = std::move(waiting);
  state->active = std::move(active);
}

void ServiceWorkerProviderContext::TakeRegistrationForServiceWorkerGlobalScope(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr* info,
    ServiceWorkerVersionAttributes* attrs) {
  DCHECK(!main_thread_task_runner_->RunsTasksInCurrentSequence());
  ControllerState* state = controller_state_.get();
  DCHECK(state);
  DCHECK(state->registration);
  DCHECK(state->registration->host_ptr_info.is_valid());
  *info = blink::mojom::ServiceWorkerRegistrationObjectInfo::New(
      state->registration->registration_id, state->registration->handle_id,
      state->registration->options->Clone(),
      std::move(state->registration->host_ptr_info));

  if (state->installing)
    attrs->installing = state->installing->info();
  if (state->waiting)
    attrs->waiting = state->waiting->info();
  if (state->active)
    attrs->active = state->active->info();
}

ServiceWorkerHandleReference* ServiceWorkerProviderContext::controller() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(controllee_state_);
  return controllee_state_->controller.get();
}

mojom::URLLoaderFactory*
ServiceWorkerProviderContext::subresource_loader_factory() {
  DCHECK(controllee_state_);
  return controllee_state_->subresource_loader_factory.get();
}

mojom::ServiceWorkerContainerHost*
ServiceWorkerProviderContext::container_host() const {
  DCHECK_EQ(SERVICE_WORKER_PROVIDER_FOR_WINDOW, provider_type_);
  return container_host_.get();
}

void ServiceWorkerProviderContext::CountFeature(uint32_t feature) {
  // ServiceWorkerProviderContext keeps track of features in order to propagate
  // it to WebServiceWorkerProviderClient, which actually records the
  // UseCounter.
  DCHECK(controllee_state_);
  controllee_state_->used_features.insert(feature);
}

const std::set<uint32_t>& ServiceWorkerProviderContext::used_features() const {
  DCHECK(controllee_state_);
  return controllee_state_->used_features;
}

void ServiceWorkerProviderContext::SetWebServiceWorkerProvider(
    base::WeakPtr<WebServiceWorkerProviderImpl> provider) {
  DCHECK(controllee_state_);
  controllee_state_->web_service_worker_provider = provider;
}

mojom::ServiceWorkerWorkerClientRequest
ServiceWorkerProviderContext::CreateWorkerClientRequest() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(controllee_state_);
  mojom::ServiceWorkerWorkerClientPtr client;
  mojom::ServiceWorkerWorkerClientRequest request = mojo::MakeRequest(&client);
  client.set_connection_error_handler(base::BindOnce(
      &ServiceWorkerProviderContext::UnregisterWorkerFetchContext,
      base::Unretained(this), client.get()));
  controllee_state_->worker_clients.push_back(std::move(client));
  return request;
}

void ServiceWorkerProviderContext::UnregisterWorkerFetchContext(
    mojom::ServiceWorkerWorkerClient* client) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(controllee_state_);
  base::EraseIf(
      controllee_state_->worker_clients,
      [client](const mojom::ServiceWorkerWorkerClientPtr& client_ptr) {
        return client_ptr.get() == client;
      });
}

void ServiceWorkerProviderContext::SetController(
    const ServiceWorkerObjectInfo& controller,
    const std::vector<blink::mojom::WebFeature>& used_features,
    bool should_notify_controllerchange) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  ControlleeState* state = controllee_state_.get();
  DCHECK(state);
  DCHECK(!state->controller ||
         state->controller->handle_id() != kInvalidServiceWorkerHandleId);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();

  state->controller = dispatcher->Adopt(controller);

  // Propagate the controller to workers related to this provider.
  if (state->controller) {
    for (const auto& worker : state->worker_clients) {
      // This is a Mojo interface call to the (dedicated or shared) worker
      // thread.
      worker->SetControllerServiceWorker(state->controller->version_id());
    }
  }
  for (blink::mojom::WebFeature feature : used_features)
    state->used_features.insert(static_cast<uint32_t>(feature));

  // S13nServiceWorker
  // Set up the URL loader factory for sending URL requests to the controller.
  if (!ServiceWorkerUtils::IsServicificationEnabled() || !state->controller) {
    state->controller_connector = nullptr;
    state->subresource_loader_factory = nullptr;
  } else {
    storage::mojom::BlobRegistryPtr blob_registry_ptr;
    ChildThreadImpl::current()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName, mojo::MakeRequest(&blob_registry_ptr));
    auto blob_registry = base::MakeRefCounted<
        base::RefCountedData<storage::mojom::BlobRegistryPtr>>();
    blob_registry->data = std::move(blob_registry_ptr);
    state->controller_connector =
        base::MakeRefCounted<ControllerServiceWorkerConnector>(
            container_host_.get());
    mojo::MakeStrongBinding(
        base::MakeUnique<ServiceWorkerSubresourceLoaderFactory>(
            state->controller_connector, state->default_loader_factory_getter,
            state->controller->url().GetOrigin(), std::move(blob_registry)),
        mojo::MakeRequest(&state->subresource_loader_factory));
  }

  // The WebServiceWorkerProviderImpl might not exist yet because the document
  // has not yet been created (as WebServiceWorkerImpl is created for a
  // ServiceWorkerContainer). In that case, once it's created it will still get
  // the controller from |this| via WebServiceWorkerProviderImpl::SetClient().
  if (state->web_service_worker_provider) {
    state->web_service_worker_provider->SetController(
        controller, state->used_features, should_notify_controllerchange);
  }
}

void ServiceWorkerProviderContext::OnNetworkProviderDestroyed() {
  container_host_.reset();
  if (controllee_state_ && controllee_state_->controller_connector)
    controllee_state_->controller_connector->OnContainerHostConnectionClosed();
}

void ServiceWorkerProviderContext::DestructOnMainThread() const {
  if (!main_thread_task_runner_->RunsTasksInCurrentSequence() &&
      main_thread_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

}  // namespace content
