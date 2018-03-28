// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/system_tray_model.h"

#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/tracing_model.h"
#include "base/logging.h"

namespace ash {

SystemTrayModel::SystemTrayModel()
    : enterprise_domain_(std::make_unique<EnterpriseDomainModel>()),
      tracing_(std::make_unique<TracingModel>()) {}

SystemTrayModel::~SystemTrayModel() = default;

void SystemTrayModel::SetClient(mojom::SystemTrayClientPtr client) {
  NOTIMPLEMENTED();
}

void SystemTrayModel::SetPrimaryTrayEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void SystemTrayModel::SetPrimaryTrayVisible(bool visible) {
  NOTIMPLEMENTED();
}

void SystemTrayModel::SetUse24HourClock(bool use_24_hour) {
  NOTIMPLEMENTED();
}

void SystemTrayModel::SetEnterpriseDisplayDomain(
    const std::string& enterprise_display_domain,
    bool active_directory_managed) {
  enterprise_domain()->SetEnterpriseDisplayDomain(enterprise_display_domain,
                                                  active_directory_managed);
}

void SystemTrayModel::SetPerformanceTracingIconVisible(bool visible) {
  tracing()->SetIsTracing(visible);
}

void SystemTrayModel::ShowUpdateIcon(mojom::UpdateSeverity severity,
                                     bool factory_reset_required,
                                     mojom::UpdateType update_type) {
  NOTIMPLEMENTED();
}

void SystemTrayModel::SetUpdateOverCellularAvailableIconVisible(bool visible) {
  NOTIMPLEMENTED();
}

}  // namespace ash
