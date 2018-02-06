// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/controller_service_worker_connector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

ControllerServiceWorkerConnector::ControllerServiceWorkerConnector(
    mojom::ServiceWorkerContainerHost* container_host)
    : container_host_(container_host) {}

ControllerServiceWorkerConnector::ControllerServiceWorkerConnector(
    mojom::ServiceWorkerContainerHost* container_host,
    mojom::ControllerServiceWorkerPtr controller_ptr)
    : container_host_(container_host) {
  ResetControllerConnection(std::move(controller_ptr));
}

mojom::ControllerServiceWorker*
ControllerServiceWorkerConnector::GetControllerServiceWorker() {
  switch (state_) {
    case State::kDisconnected:
      DCHECK(!controller_service_worker_);
      DCHECK(container_host_);
      container_host_->GetControllerServiceWorker(
          mojo::MakeRequest(&controller_service_worker_));
      controller_service_worker_.set_connection_error_handler(base::BindOnce(
          &ControllerServiceWorkerConnector::OnControllerConnectionClosed,
          base::Unretained(this)));
      state_ = State::kConnected;
      return controller_service_worker_.get();
    case State::kConnected:
      DCHECK(controller_service_worker_.is_bound());
      return controller_service_worker_.get();
    case State::kNoController:
      DCHECK(!controller_service_worker_);
      return nullptr;
    case State::kNoContainerHost:
      DCHECK(!controller_service_worker_);
      DCHECK(!container_host_);
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
  container_host_ = nullptr;
  controller_service_worker_.reset();
}

void ControllerServiceWorkerConnector::OnControllerConnectionClosed() {
  DCHECK_EQ(State::kConnected, state_);
  state_ = State::kDisconnected;
  controller_service_worker_.reset();
  for (auto& observer : observer_list_)
    observer.OnConnectionClosed();
}

void ControllerServiceWorkerConnector::ResetControllerConnection(
    mojom::ControllerServiceWorkerPtr controller_ptr) {
  if (state_ == State::kNoContainerHost)
    return;
  controller_service_worker_ = std::move(controller_ptr);
  if (controller_service_worker_) {
    state_ = State::kConnected;
    controller_service_worker_.set_connection_error_handler(base::BindOnce(
        &ControllerServiceWorkerConnector::OnControllerConnectionClosed,
        base::Unretained(this)));
  } else {
    state_ = State::kNoController;
  }
}

ControllerServiceWorkerConnector::~ControllerServiceWorkerConnector() = default;

}  // namespace content
