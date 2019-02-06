// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_FRAME_RESOURCE_COORDINATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_FRAME_RESOURCE_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/performance_manager/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class ProcessResourceCoordinator;

class FrameResourceCoordinator
    : public ResourceCoordinatorInterface<
          resource_coordinator::mojom::FrameCoordinationUnitPtr,
          resource_coordinator::mojom::FrameCoordinationUnitRequest> {
 public:
  explicit FrameResourceCoordinator(PerformanceManager* performance_manager);
  ~FrameResourceCoordinator() override;

  void SetProcess(const ProcessResourceCoordinator& process);
  void AddChildFrame(const FrameResourceCoordinator& child);
  void RemoveChildFrame(const FrameResourceCoordinator& child);

  // Closes the connection to the service.
  void reset() { service_.reset(); }

 private:
  void ConnectToService(
      resource_coordinator::mojom::CoordinationUnitProviderPtr& provider,
      const resource_coordinator::CoordinationUnitID& cu_id) override;

  void SetProcessByID(
      const resource_coordinator::CoordinationUnitID& process_id);
  void AddChildFrameByID(
      const resource_coordinator::CoordinationUnitID& child_id);
  void RemoveChildFrameByID(
      const resource_coordinator::CoordinationUnitID& child_id);

  THREAD_CHECKER(thread_checker_);

  // The WeakPtrFactory should come last so the weak ptrs are invalidated
  // before the rest of the member variables.
  base::WeakPtrFactory<FrameResourceCoordinator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameResourceCoordinator);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_FRAME_RESOURCE_COORDINATOR_H_
