// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/accelerator_registrar_impl.h"

#include <stdint.h>
#include <utility>

#include "ash/mus/root_window_controller.h"
#include "ash/mus/window_manager.h"
#include "base/bind.h"
#include "components/mus/public/cpp/window_manager_delegate.h"

namespace ash {
namespace mus {

namespace {
const int kAcceleratorIdMask = 0xffff;

void CallAddAcceleratorCallback(
    const ::mus::mojom::AcceleratorRegistrar::AddAcceleratorCallback& callback,
    bool result) {
  callback.Run(result);
}

}  // namespace

AcceleratorRegistrarImpl::AcceleratorRegistrarImpl(
    WindowManager* window_manager,
    uint32_t accelerator_namespace,
    mojo::InterfaceRequest<AcceleratorRegistrar> request,
    const DestroyCallback& destroy_callback)
    : window_manager_(window_manager),
      binding_(this, std::move(request)),
      accelerator_namespace_(accelerator_namespace & 0xffff),
      destroy_callback_(destroy_callback) {
  window_manager_->AddObserver(this);
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
                                                  const ui::Event& event) {
  DCHECK(OwnsAccelerator(accelerator_id));
  // TODO(moshayedi): crbug.com/617167. Don't clone even once we map
  // mojom::Event directly to ui::Event.
  accelerator_handler_->OnAccelerator(accelerator_id & kAcceleratorIdMask,
                                      ui::Event::Clone(event));
}

AcceleratorRegistrarImpl::~AcceleratorRegistrarImpl() {
  window_manager_->RemoveObserver(this);
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

void AcceleratorRegistrarImpl::RemoveAllAccelerators() {
  for (uint32_t accelerator : accelerators_)
    window_manager_->window_manager_client()->RemoveAccelerator(accelerator);

  accelerators_.clear();
}

void AcceleratorRegistrarImpl::SetHandler(
    ::mus::mojom::AcceleratorHandlerPtr handler) {
  accelerator_handler_ = std::move(handler);
  accelerator_handler_.set_connection_error_handler(base::Bind(
      &AcceleratorRegistrarImpl::OnHandlerGone, base::Unretained(this)));
}

void AcceleratorRegistrarImpl::AddAccelerator(
    uint32_t accelerator_id,
    ::mus::mojom::EventMatcherPtr matcher,
    const AddAcceleratorCallback& callback) {
  if (!accelerator_handler_ ||
      (accelerator_id & kAcceleratorIdMask) != accelerator_id) {
    // The |accelerator_id| is too large, and it can't be handled correctly.
    callback.Run(false);
    return;
  }
  uint32_t namespaced_accelerator_id = ComputeAcceleratorId(accelerator_id);
  accelerators_.insert(namespaced_accelerator_id);
  window_manager_->window_manager_client()->AddAccelerator(
      namespaced_accelerator_id, std::move(matcher),
      base::Bind(&CallAddAcceleratorCallback, callback));
}

void AcceleratorRegistrarImpl::RemoveAccelerator(uint32_t accelerator_id) {
  uint32_t namespaced_accelerator_id = ComputeAcceleratorId(accelerator_id);
  if (accelerators_.erase(namespaced_accelerator_id) == 0)
    return;
  window_manager_->window_manager_client()->RemoveAccelerator(
      namespaced_accelerator_id);

  // If the registrar is not bound anymore (i.e. the client can no longer
  // install new accelerators), and the last accelerator has been removed, then
  // there's no point keeping this alive anymore.
  if (accelerators_.empty() && !binding_.is_bound())
    delete this;
}

void AcceleratorRegistrarImpl::OnAccelerator(uint32_t id,
                                             const ui::Event& event) {
  if (OwnsAccelerator(id))
    ProcessAccelerator(id, event);
}

void AcceleratorRegistrarImpl::OnWindowTreeClientDestroyed() {
  delete this;
}

}  // namespace mus
}  // namespace ash
