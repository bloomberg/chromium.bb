// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_H_
#define COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_H_

#include "base/memory/singleton.h"
#include "base/process/process_handle.h"
#include "components/memory_coordinator/common/client_registry.h"
#include "components/memory_coordinator/common/memory_coordinator_export.h"
#include "components/memory_coordinator/public/interfaces/memory_coordinator.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace memory_coordinator {

class MemoryCoordinatorHandleImpl;

// MemoryCoordinator is responsible for the whole memory management accross the
// browser and child proceeses. It dispatches memory events to its clients and
// child processes based on its best knowledge of the memory usage.
class MEMORY_COORDINATOR_EXPORT MemoryCoordinator : public ClientRegistry {
 public:
  ~MemoryCoordinator() override;

  // Singleton factory/accessor.
  static MemoryCoordinator* GetInstance();

  // Creates a handle to the provided child process.
  void CreateHandle(int render_process_id,
                    mojom::MemoryCoordinatorHandleRequest request);

  // Returns number of children. Only used for testing.
  size_t NumChildrenForTesting();

  // Dispatches a memory state change to the provided process. Returns true if
  // the process is tracked by this coordinator and successfully dispatches,
  // returns false otherwise.
  bool SetMemoryState(
      int render_process_id, mojom::MemoryState memory_state);

  // Returns the memory state of the specified render process. Returns UNKNOWN
  // if the process is not tracked by this coordinator.
  mojom::MemoryState GetMemoryState(int render_process_id) const;

 protected:
  // Constructor. Protected as this is a singleton, but accessible for
  // unittests.
  MemoryCoordinator();

  // Adds the given ChildMemoryCoordinator as a child of this coordinator.
  void AddChildForTesting(int dummy_render_process_id,
                          mojom::ChildMemoryCoordinatorPtr child);

  // Callback invoked by mojo when the child connection goes down. Exposed
  // for testing.
  void OnConnectionError(int render_process_id);

 private:
  friend struct base::DefaultSingletonTraits<MemoryCoordinator>;

  // Helper function of CreateHandle and AddChildForTesting.
  void CreateChildInfoMapEntry(
      int render_process_id,
      std::unique_ptr<MemoryCoordinatorHandleImpl> handle);

  // Stores information about any known child processes.
  struct ChildInfo {
    // This object must be compatible with STL containers.
    ChildInfo();
    ChildInfo(const ChildInfo& rhs);
    ~ChildInfo();

    mojom::MemoryState memory_state;
    std::unique_ptr<MemoryCoordinatorHandleImpl> handle;
  };

  // A map from process ID (RenderProcessHost::GetID()) to child process info.
  using ChildInfoMap = std::map<int, ChildInfo>;

  // Tracks child processes. An entry is added when a renderer connects to
  // MemoryCoordinator and removed automatically when an underlying binding is
  // disconnected.
  ChildInfoMap children_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinator);
};

}  // memory_coordinator

#endif  // COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_H_
