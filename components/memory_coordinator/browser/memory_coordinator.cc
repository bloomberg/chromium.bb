// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/browser/memory_coordinator.h"

namespace memory_coordinator {

// The implementation of MemoryCoordinatorHandle. See memory_coordinator.mojom
// for the role of this class.
class MemoryCoordinatorHandleImpl : public mojom::MemoryCoordinatorHandle {
 public:
  MemoryCoordinatorHandleImpl(mojom::MemoryCoordinatorHandleRequest request)
      : binding_(this, std::move(request)) {
  }

  // mojom::MemoryCoordinatorHandle implementations:

  void AddChild(mojom::ChildMemoryCoordinatorPtr child) override {
    DCHECK(!child_.is_bound());
    child_ = std::move(child);
  }

  mojom::ChildMemoryCoordinatorPtr& child() { return child_; }
  mojo::Binding<mojom::MemoryCoordinatorHandle>& binding() { return binding_; }

 private:
  mojom::ChildMemoryCoordinatorPtr child_;
  mojo::Binding<mojom::MemoryCoordinatorHandle> binding_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorHandleImpl);
};

MemoryCoordinator::MemoryCoordinator()
    : pressure_listener_(
          base::Bind(&MemoryCoordinator::OnMemoryPressure,
                     base::Unretained(this))) {}

MemoryCoordinator::~MemoryCoordinator() {}

void MemoryCoordinator::CreateHandle(
    int render_process_id,
    mojom::MemoryCoordinatorHandleRequest request) {
  auto* handle = new MemoryCoordinatorHandleImpl(std::move(request));
  handle->binding().set_connection_error_handler(
      base::Bind(&MemoryCoordinator::OnConnectionError, base::Unretained(this),
                 render_process_id));
  children_[render_process_id].reset(handle);
}

size_t MemoryCoordinator::NumChildrenForTesting() {
  return children_.size();
}

void MemoryCoordinator::OnConnectionError(int render_process_id) {
  children_.erase(render_process_id);
}

void MemoryCoordinator::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  // TODO(bashi): The current implementation just translates memory pressure
  // levels to memory coordinator states. The logic will be replaced with
  // the priority tracker.
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    clients()->Notify(FROM_HERE, &MemoryCoordinatorClient::OnMemoryStateChange,
                      mojom::MemoryState::THROTTLED);
  } else if (level ==
             base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    clients()->Notify(FROM_HERE, &MemoryCoordinatorClient::OnMemoryStateChange,
                      mojom::MemoryState::SUSPENDED);
  }
}

}  // namespace memory_coordinator
