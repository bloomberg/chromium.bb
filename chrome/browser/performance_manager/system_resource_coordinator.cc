// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/system_resource_coordinator.h"

namespace resource_coordinator {

SystemResourceCoordinator::SystemResourceCoordinator(
    PerformanceManager* performance_manager)
    : ResourceCoordinatorInterface() {
  CoordinationUnitID new_cu_id(CoordinationUnitType::kSystem,
                               CoordinationUnitID::RANDOM_ID);
  ResourceCoordinatorInterface::ConnectToService(performance_manager,
                                                 new_cu_id);
}

SystemResourceCoordinator::~SystemResourceCoordinator() = default;

void SystemResourceCoordinator::DistributeMeasurementBatch(
    mojom::ProcessResourceMeasurementBatchPtr batch) {
  if (!service_)
    return;
  service_->DistributeMeasurementBatch(std::move(batch));
}

void SystemResourceCoordinator::ConnectToService(
    mojom::CoordinationUnitProviderPtr& provider,
    const CoordinationUnitID& cu_id) {
  provider->GetSystemCoordinationUnit(mojo::MakeRequest(&service_));
}

}  // namespace resource_coordinator
