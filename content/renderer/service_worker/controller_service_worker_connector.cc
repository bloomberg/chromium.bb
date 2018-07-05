// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/controller_service_worker_connector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace content {

ControllerServiceWorkerConnector::ControllerServiceWorkerConnector(
    mojom::ServiceWorkerContainerHostPtrInfo container_host_info,
    mojom::ControllerServiceWorkerPtrInfo controller_info,
    const std::string& client_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : client_id_(client_id), weak_factory_(this) {
  DCHECK(task_runner);
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ControllerServiceWorkerConnector::InitializeOnTaskRunner,
                     base::Unretained(this), std::move(container_host_info),
                     std::move(controller_info)));
}

ControllerServiceWorkerConnector::~ControllerServiceWorkerConnector() = default;

mojom::ControllerServiceWorker*
ControllerServiceWorkerConnector::GetControllerServiceWorker(
    mojom::ControllerServiceWorkerPurpose purpose) {
  switch (state_) {
    case State::kDisconnected: {
      DCHECK(!controller_service_worker_);
      DCHECK(container_host_ptr_);
      mojom::ControllerServiceWorkerPtr controller_ptr;
      container_host_ptr_->EnsureControllerServiceWorker(
          mojo::MakeRequest(&controller_ptr), purpose);
      SetControllerServiceWorkerPtr(std::move(controller_ptr));
      return controller_service_worker_.get();
    }
    case State::kConnected:
      DCHECK(controller_service_worker_.is_bound());
      return controller_service_worker_.get();
    case State::kNoController:
      DCHECK(!controller_service_worker_);
      return nullptr;
    case State::kNoContainerHost:
      DCHECK(!controller_service_worker_);
      DCHECK(!container_host_ptr_);
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

void ControllerServiceWorkerConnector::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ControllerServiceWorkerConnector::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ControllerServiceWorkerConnector::OnContainerHostConnectionClosed() {
  state_ = State::kNoContainerHost;
  container_host_ptr_.reset();
  controller_service_worker_.reset();
}

void ControllerServiceWorkerConnector::OnControllerConnectionClosed() {
  DCHECK_EQ(State::kConnected, state_);
  state_ = State::kDisconnected;
  controller_service_worker_.reset();
  for (auto& observer : observer_list_)
    observer.OnConnectionClosed();
}

void ControllerServiceWorkerConnector::UpdateController(
    mojom::ControllerServiceWorkerPtrInfo controller_info) {
  if (state_ == State::kNoContainerHost)
    return;
  SetControllerServiceWorkerPtr(
      mojom::ControllerServiceWorkerPtr(std::move(controller_info)));
  if (!controller_service_worker_)
    state_ = State::kNoController;
}

void ControllerServiceWorkerConnector::InitializeOnTaskRunner(
    mojom::ServiceWorkerContainerHostPtrInfo container_host_info,
    mojom::ControllerServiceWorkerPtrInfo controller_info) {
  DCHECK(!container_host_ptr_);
  container_host_ptr_.Bind(std::move(container_host_info));
  container_host_ptr_.set_connection_error_handler(base::BindOnce(
      &ControllerServiceWorkerConnector::OnContainerHostConnectionClosed,
      base::Unretained(this)));
  SetControllerServiceWorkerPtr(
      mojom::ControllerServiceWorkerPtr(std::move(controller_info)));
}

void ControllerServiceWorkerConnector::SetControllerServiceWorkerPtr(
    mojom::ControllerServiceWorkerPtr controller_ptr) {
  controller_service_worker_ = std::move(controller_ptr);
  if (controller_service_worker_) {
    controller_service_worker_.set_connection_error_handler(base::BindOnce(
        &ControllerServiceWorkerConnector::OnControllerConnectionClosed,
        base::Unretained(this)));
    state_ = State::kConnected;
  }
}

}  // namespace content
