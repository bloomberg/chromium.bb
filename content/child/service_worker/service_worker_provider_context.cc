// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_provider_context.h"

#include <utility>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_registry.h"

namespace content {

// Holds state for service worker clients.
struct ServiceWorkerProviderContext::ControlleeState {
  ControlleeState() = default;
  ~ControlleeState() = default;

  std::unique_ptr<ServiceWorkerHandleReference> controller;

  // Used to dispatch events to the controller.
  mojom::ServiceWorkerEventDispatcherPtr event_dispatcher;

  // Tracks feature usage for UseCounter.
  std::set<uint32_t> used_features;
};

// Holds state for service worker execution contexts.
struct ServiceWorkerProviderContext::ControllerState {
  ControllerState() = default;
  ~ControllerState() = default;
  std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration;
  std::unique_ptr<ServiceWorkerHandleReference> installing;
  std::unique_ptr<ServiceWorkerHandleReference> waiting;
  std::unique_ptr<ServiceWorkerHandleReference> active;
};

ServiceWorkerProviderContext::ServiceWorkerProviderContext(
    int provider_id,
    ServiceWorkerProviderType provider_type,
    mojom::ServiceWorkerProviderAssociatedRequest request,
    ServiceWorkerDispatcher* dispatcher)
    : provider_id_(provider_id),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this, std::move(request)) {
  if (provider_type == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER)
    controller_state_ = base::MakeUnique<ControllerState>();
  else
    controllee_state_ = base::MakeUnique<ControlleeState>();

  dispatcher->AddProviderContext(this);
}

ServiceWorkerProviderContext::~ServiceWorkerProviderContext() {
  if (ServiceWorkerDispatcher* dispatcher =
          ServiceWorkerDispatcher::GetThreadSpecificInstance()) {
    // Remove this context from the dispatcher living on the main thread.
    dispatcher->RemoveProviderContext(this);
  }
}

void ServiceWorkerProviderContext::SetRegistration(
    std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration,
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

void ServiceWorkerProviderContext::GetRegistration(
    ServiceWorkerRegistrationObjectInfo* info,
    ServiceWorkerVersionAttributes* attrs) {
  DCHECK(!main_thread_task_runner_->RunsTasksInCurrentSequence());
  ControllerState* state = controller_state_.get();
  DCHECK(state);
  DCHECK(state->registration);
  *info = state->registration->info();
  if (state->installing)
    attrs->installing = state->installing->info();
  if (state->waiting)
    attrs->waiting = state->waiting->info();
  if (state->active)
    attrs->active = state->active->info();
}

void ServiceWorkerProviderContext::SetController(
    std::unique_ptr<ServiceWorkerHandleReference> controller,
    const std::set<uint32_t>& used_features,
    mojom::ServiceWorkerEventDispatcherPtrInfo event_dispatcher_ptr_info) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  ControlleeState* state = controllee_state_.get();
  DCHECK(state);
  DCHECK(!state->controller ||
         state->controller->handle_id() != kInvalidServiceWorkerHandleId);
  state->controller = std::move(controller);
  state->used_features = used_features;
  if (event_dispatcher_ptr_info.is_valid())
    state->event_dispatcher.Bind(std::move(event_dispatcher_ptr_info));
}

ServiceWorkerHandleReference* ServiceWorkerProviderContext::controller() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(controllee_state_);
  return controllee_state_->controller.get();
}

mojom::ServiceWorkerEventDispatcher*
ServiceWorkerProviderContext::event_dispatcher() {
  DCHECK(controllee_state_);
  return controllee_state_->event_dispatcher.get();
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

void ServiceWorkerProviderContext::DestructOnMainThread() const {
  if (!main_thread_task_runner_->RunsTasksInCurrentSequence() &&
      main_thread_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

}  // namespace content
