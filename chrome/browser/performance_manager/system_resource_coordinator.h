// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_SYSTEM_RESOURCE_COORDINATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_SYSTEM_RESOURCE_COORDINATOR_H_

#include "chrome/browser/performance_manager/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace resource_coordinator {

class SystemResourceCoordinator : public ResourceCoordinatorInterface<
                                      mojom::SystemCoordinationUnitPtr,
                                      mojom::SystemCoordinationUnitRequest> {
 public:
  explicit SystemResourceCoordinator(PerformanceManager* performance_manager);
  ~SystemResourceCoordinator() override;

  void DistributeMeasurementBatch(
      mojom::ProcessResourceMeasurementBatchPtr batch);

 private:
  void ConnectToService(mojom::CoordinationUnitProviderPtr& provider,
                        const CoordinationUnitID& cu_id) override;

  DISALLOW_COPY_AND_ASSIGN(SystemResourceCoordinator);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_SYSTEM_RESOURCE_COORDINATOR_H_
