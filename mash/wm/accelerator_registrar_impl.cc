// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/accelerator_registrar_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "mash/wm/root_window_controller.h"
#include "mash/wm/window_manager.h"
#include "mash/wm/window_manager_application.h"

namespace mash {
namespace wm {

namespace {
const int kAcceleratorIdMask = 0xffff;

void OnAcceleratorAdded(bool result) {}
void CallAddAcceleratorCallback(
    const mus::mojom::AcceleratorRegistrar::AddAcceleratorCallback& callback,
    bool result) {
  callback.Run(result);
}

}  // namespace

struct AcceleratorRegistrarImpl::Accelerator {
  mus::mojom::EventMatcherPtr event_matcher;
  AddAcceleratorCallback callback;
  bool callback_used = false;
};

AcceleratorRegistrarImpl::AcceleratorRegistrarImpl(
    WindowManagerApplication* wm_app,
    uint32_t accelerator_namespace,
    mojo::InterfaceRequest<AcceleratorRegistrar> request,
    const DestroyCallback& destroy_callback)
    : wm_app_(wm_app),
      binding_(this, std::move(request)),
      accelerator_namespace_(accelerator_namespace & 0xffff),
      destroy_callback_(destroy_callback) {
  wm_app_->AddRootWindowsObserver(this);
  binding_.set_connection_error_handler(base::Bind(
      &AcceleratorRegistrarImpl::OnBindingGone, base::Unretained(this)));
}

void AcceleratorRegistrarImpl::Destroy() {
  delete this;
}

bool AcceleratorRegistrarImpl::OwnsAccelerator(uint32_t accelerator_id) const {
  return !!accelerators_.count(accelerator_id);
}

void AcceleratorRegistrarImpl::ProcessAccelerator(uint32_t accelerator_id,
                                                  mus::mojom::EventPtr event) {
  DCHECK(OwnsAccelerator(accelerator_id));
  accelerator_handler_->OnAccelerator(accelerator_id & kAcceleratorIdMask,
                                      std::move(event));
}

AcceleratorRegistrarImpl::~AcceleratorRegistrarImpl() {
  wm_app_->RemoveRootWindowsObserver(this);
  RemoveAllAccelerators();
  destroy_callback_.Run(this);
}

uint32_t AcceleratorRegistrarImpl::ComputeAcceleratorId(
    uint32_t accelerator_id) const {
  return (accelerator_namespace_ << 16) | (accelerator_id & kAcceleratorIdMask);
}

void AcceleratorRegistrarImpl::OnBindingGone() {
  binding_.Unbind();
  // If there's no outstanding accelerators for this connection, then destroy
  // it.
  if (accelerators_.empty())
    delete this;
}

void AcceleratorRegistrarImpl::OnHandlerGone() {
  // The handler is dead. If AcceleratorRegistrar connection is also closed,
  // then destroy this. Otherwise, remove all the accelerators, but keep the
  // AcceleratorRegistrar connection alive (the client could still set another
  // handler and install new accelerators).
  if (!binding_.is_bound()) {
    delete this;
    return;
  }
  accelerator_handler_.reset();
  RemoveAllAccelerators();
}

void AcceleratorRegistrarImpl::AddAcceleratorToRoot(
    RootWindowController* root,
    uint32_t namespaced_accelerator_id) {
  Accelerator& accelerator = accelerators_[namespaced_accelerator_id];
  AddAcceleratorCallback callback = accelerator.callback_used
                                        ? base::Bind(&OnAcceleratorAdded)
                                        : accelerator.callback;
  // Ensure we only notify the callback once (as happens with mojoms).
  accelerator.callback_used = true;
  root->window_manager()->window_manager_client()->AddAccelerator(
      namespaced_accelerator_id, accelerator.event_matcher.Clone(),
      base::Bind(&CallAddAcceleratorCallback, callback));
}

void AcceleratorRegistrarImpl::RemoveAllAccelerators() {
  for (const auto& pair : accelerators_) {
    for (RootWindowController* root : wm_app_->GetRootControllers()) {
      root->window_manager()->window_manager_client()->RemoveAccelerator(
          pair.first);
    }
  }
  accelerators_.clear();
}

void AcceleratorRegistrarImpl::SetHandler(
    mus::mojom::AcceleratorHandlerPtr handler) {
  accelerator_handler_ = std::move(handler);
  accelerator_handler_.set_connection_error_handler(base::Bind(
      &AcceleratorRegistrarImpl::OnHandlerGone, base::Unretained(this)));
}

void AcceleratorRegistrarImpl::AddAccelerator(
    uint32_t accelerator_id,
    mus::mojom::EventMatcherPtr matcher,
    const AddAcceleratorCallback& callback) {
  if (!accelerator_handler_ ||
      (accelerator_id & kAcceleratorIdMask) != accelerator_id) {
    // The |accelerator_id| is too large, and it can't be handled correctly.
    callback.Run(false);
    return;
  }
  uint32_t namespaced_accelerator_id = ComputeAcceleratorId(accelerator_id);
  accelerators_[namespaced_accelerator_id].event_matcher = matcher->Clone();
  accelerators_[namespaced_accelerator_id].callback = callback;
  accelerators_[namespaced_accelerator_id].callback_used = false;
  for (RootWindowController* root : wm_app_->GetRootControllers())
    AddAcceleratorToRoot(root, namespaced_accelerator_id);
}

void AcceleratorRegistrarImpl::RemoveAccelerator(uint32_t accelerator_id) {
  uint32_t namespaced_accelerator_id = ComputeAcceleratorId(accelerator_id);
  auto iter = accelerators_.find(namespaced_accelerator_id);
  if (iter == accelerators_.end())
    return;
  for (RootWindowController* root : wm_app_->GetRootControllers()) {
    root->window_manager()->window_manager_client()->RemoveAccelerator(
        namespaced_accelerator_id);
  }
  accelerators_.erase(iter);
  // If the registrar is not bound anymore (i.e. the client can no longer
  // install new accelerators), and the last accelerator has been removed, then
  // there's no point keeping this alive anymore.
  if (accelerators_.empty() && !binding_.is_bound())
    delete this;
}

void AcceleratorRegistrarImpl::OnRootWindowControllerAdded(
    RootWindowController* controller) {
  for (const auto& pair : accelerators_)
    AddAcceleratorToRoot(controller, pair.first);
}

}  // namespace wm
}  // namespace mash
