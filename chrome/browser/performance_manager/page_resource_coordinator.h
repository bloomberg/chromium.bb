// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PAGE_RESOURCE_COORDINATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PAGE_RESOURCE_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/frame_resource_coordinator.h"
#include "chrome/browser/performance_manager/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class PageResourceCoordinator
    : public ResourceCoordinatorInterface<
          resource_coordinator::mojom::PageCoordinationUnitPtr,
          resource_coordinator::mojom::PageCoordinationUnitRequest> {
 public:
  explicit PageResourceCoordinator(PerformanceManager* performance_manager);
  ~PageResourceCoordinator() override;

  void SetIsLoading(bool is_loading);
  void SetVisibility(bool visible);
  void SetUKMSourceId(int64_t ukm_source_id);
  void OnFaviconUpdated();
  void OnTitleUpdated();
  void OnMainFrameNavigationCommitted(base::TimeTicks navigation_committed_time,
                                      uint64_t navigation_id,
                                      const std::string& url);

  void AddFrame(const FrameResourceCoordinator& frame);
  void RemoveFrame(const FrameResourceCoordinator& frame);

 private:
  void ConnectToService(
      resource_coordinator::mojom::CoordinationUnitProviderPtr& provider,
      const resource_coordinator::CoordinationUnitID& cu_id) override;

  void AddFrameByID(const resource_coordinator::CoordinationUnitID& cu_id);
  void RemoveFrameByID(const resource_coordinator::CoordinationUnitID& cu_id);

  THREAD_CHECKER(thread_checker_);

  // The WeakPtrFactory should come last so the weak ptrs are invalidated
  // before the rest of the member variables.
  base::WeakPtrFactory<PageResourceCoordinator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageResourceCoordinator);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PAGE_RESOURCE_COORDINATOR_H_
