// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_COORDINATION_UNIT_SYSTEM_COORDINATION_UNIT_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_COORDINATION_UNIT_SYSTEM_COORDINATION_UNIT_IMPL_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/coordination_unit/coordination_unit_base.h"

namespace performance_manager {

class SystemCoordinationUnitImpl
    : public CoordinationUnitInterface<
          SystemCoordinationUnitImpl,
          resource_coordinator::mojom::SystemCoordinationUnit,
          resource_coordinator::mojom::SystemCoordinationUnitRequest> {
 public:
  static resource_coordinator::CoordinationUnitType Type() {
    return resource_coordinator::CoordinationUnitType::kSystem;
  }

  SystemCoordinationUnitImpl(
      const resource_coordinator::CoordinationUnitID& id,
      CoordinationUnitGraph* graph,
      std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref);
  ~SystemCoordinationUnitImpl() override;

  // resource_coordinator::mojom::SystemCoordinationUnit implementation:
  void OnProcessCPUUsageReady() override;
  void DistributeMeasurementBatch(
      resource_coordinator::mojom::ProcessResourceMeasurementBatchPtr
          measurement_batch) override;

  // Accessors for the start/end times bracketing when the last performance
  // measurement occurred.
  base::TimeTicks last_measurement_start_time() const {
    return last_measurement_start_time_;
  }
  base::TimeTicks last_measurement_end_time() const {
    return last_measurement_end_time_;
  }

 private:
  base::TimeTicks last_measurement_start_time_;
  base::TimeTicks last_measurement_end_time_;

  // CoordinationUnitInterface implementation:
  void OnEventReceived(resource_coordinator::mojom::Event event) override;
  void OnPropertyChanged(
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override;

  DISALLOW_COPY_AND_ASSIGN(SystemCoordinationUnitImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_COORDINATION_UNIT_SYSTEM_COORDINATION_UNIT_IMPL_H_
