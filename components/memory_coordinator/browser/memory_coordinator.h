// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_H_
#define COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_H_

#include "base/memory/memory_pressure_listener.h"
#include "components/memory_coordinator/common/client_registry.h"
#include "components/memory_coordinator/common/memory_coordinator_export.h"
#include "components/memory_coordinator/public/interfaces/memory_coordinator.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace memory_coordinator {

class MemoryCoordinatorHandleImpl;

// MemoryCoordinator is responsible for the whole memory management accross the
// browser and child proceeses. It will dispatch memory events to its clients
// and child processes based on its best knowledge of the memory usage.
class MEMORY_COORDINATOR_EXPORT MemoryCoordinator : public ClientRegistry {
 public:
  MemoryCoordinator();
  ~MemoryCoordinator() override;

  void CreateHandle(int render_process_id,
                    mojom::MemoryCoordinatorHandleRequest request);

  // Returns number of children. Only used for testing.
  size_t NumChildrenForTesting();

 private:
  void OnConnectionError(int render_process_id);

  // Called when MemoryPressureListener detects memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Mappings of RenderProcessHost::GetID() -> MemoryCoordinatorHandleImpl.
  // A mapping is added when a renderer connects to MemoryCoordinator and
  // removed automatically when a underlying binding is disconnected.
  std::map<int, std::unique_ptr<MemoryCoordinatorHandleImpl>> children_;

  base::MemoryPressureListener pressure_listener_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinator);
};

}  // memory_coordinator

#endif  // COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_H_
