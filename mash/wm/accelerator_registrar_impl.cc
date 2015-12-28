// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/accelerator_registrar_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"

namespace mash {
namespace wm {

namespace {
const int kAcceleratorIdMask = 0xffff;
}

AcceleratorRegistrarImpl::AcceleratorRegistrarImpl(
    mus::mojom::WindowTreeHost* host,
    uint32_t accelerator_namespace,
    mojo::InterfaceRequest<AcceleratorRegistrar> request,
    const DestroyCallback& destroy_callback)
    : host_(host),
      binding_(this, std::move(request)),
      accelerator_namespace_(accelerator_namespace & 0xffff),
      destroy_callback_(destroy_callback) {
  binding_.set_connection_error_handler(base::Bind(
      &AcceleratorRegistrarImpl::OnBindingGone, base::Unretained(this)));
}

AcceleratorRegistrarImpl::~AcceleratorRegistrarImpl() {
  for (uint32_t accelerator_id : accelerator_ids_)
    host_->RemoveAccelerator(accelerator_id);
  destroy_callback_.Run(this);
}

bool AcceleratorRegistrarImpl::OwnsAccelerator(uint32_t accelerator_id) const {
  return !!accelerator_ids_.count(accelerator_id);
}

void AcceleratorRegistrarImpl::ProcessAccelerator(uint32_t accelerator_id,
                                                  mus::mojom::EventPtr event) {
  DCHECK(OwnsAccelerator(accelerator_id));
  accelerator_handler_->OnAccelerator(accelerator_id & kAcceleratorIdMask,
                                      std::move(event));
}

uint32_t AcceleratorRegistrarImpl::ComputeAcceleratorId(
    uint32_t accelerator_id) const {
  return (accelerator_namespace_ << 16) | (accelerator_id & kAcceleratorIdMask);
}

void AcceleratorRegistrarImpl::OnBindingGone() {
  binding_.Unbind();
  // If there's no outstanding accelerators for this connection, then destroy
  // it.
  if (accelerator_ids_.empty())
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
  for (uint32_t accelerator_id : accelerator_ids_)
    host_->RemoveAccelerator(accelerator_id);
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
  accelerator_ids_.insert(namespaced_accelerator_id);
  host_->AddAccelerator(namespaced_accelerator_id, std::move(matcher),
                        callback);
}

void AcceleratorRegistrarImpl::RemoveAccelerator(uint32_t accelerator_id) {
  uint32_t namespaced_accelerator_id = ComputeAcceleratorId(accelerator_id);
  if (!accelerator_ids_.count(namespaced_accelerator_id))
    return;
  host_->RemoveAccelerator(namespaced_accelerator_id);
  accelerator_ids_.erase(namespaced_accelerator_id);
  // If the registrar is not bound anymore (i.e. the client can no longer
  // install new accelerators), and the last accelerator has been removed, then
  // there's no point keeping this alive anymore.
  if (accelerator_ids_.empty() && !binding_.is_bound())
    delete this;
}

}  // namespace wm
}  // namespace mash
