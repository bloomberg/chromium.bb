// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_COORDINATOR_CHILD_CHILD_MEMORY_COORDINATOR_IMPL_H_
#define COMPONENTS_MEMORY_COORDINATOR_CHILD_CHILD_MEMORY_COORDINATOR_IMPL_H_

#include "base/compiler_specific.h"
#include "components/memory_coordinator/common/client_registry.h"
#include "components/memory_coordinator/common/memory_coordinator_client.h"
#include "components/memory_coordinator/common/memory_coordinator_features.h"
#include "components/memory_coordinator/public/interfaces/child_memory_coordinator.mojom.h"
#include "components/memory_coordinator/public/interfaces/memory_coordinator.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace memory_coordinator {

// ChildMemoryCoordinatorImpl is the implementation of ChildMemoryCoordinator.
// It lives in child processes and is responsible for dispatching memory events
// to its clients.
class ChildMemoryCoordinatorImpl
    : public ClientRegistry,
      NON_EXPORTED_BASE(public mojom::ChildMemoryCoordinator) {
 public:
  explicit ChildMemoryCoordinatorImpl(mojom::MemoryCoordinatorHandlePtr parent);
  ~ChildMemoryCoordinatorImpl() override;

  // mojom::ChildMemoryCoordinator implementations:
  void OnStateChange(mojom::MemoryState state) override;

 private:
  friend class ChildMemoryCoordinatorImplTest;

  mojo::Binding<mojom::ChildMemoryCoordinator> binding_;
  mojom::MemoryCoordinatorHandlePtr parent_;

  DISALLOW_COPY_AND_ASSIGN(ChildMemoryCoordinatorImpl);
};

}  // namespace content

#endif  // COMPONENTS_MEMORY_COORDINATOR_CHILD_CHILD_MEMORY_COORDINATOR_IMPL_H_
