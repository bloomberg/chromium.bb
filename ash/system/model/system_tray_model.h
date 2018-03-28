// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_

#include <memory>

#include "ash/public/interfaces/system_tray.mojom.h"
#include "base/macros.h"

namespace ash {

class ClockModel;
class EnterpriseDomainModel;
class TracingModel;
class UpdateModel;

// Top level model of SystemTray.
// TODO(tetsui): Eventually migrate all of the mojom::SystemTray implementation
// from SystemTrayController, and bind SystemTrayRequest directly.
class SystemTrayModel : public mojom::SystemTray {
 public:
  SystemTrayModel();
  ~SystemTrayModel() override;

  // mojom::SystemTray:
  void SetClient(mojom::SystemTrayClientPtr client) override;
  void SetPrimaryTrayEnabled(bool enabled) override;
  void SetPrimaryTrayVisible(bool visible) override;
  void SetUse24HourClock(bool use_24_hour) override;
  void SetEnterpriseDisplayDomain(const std::string& enterprise_display_domain,
                                  bool active_directory_managed) override;
  void SetPerformanceTracingIconVisible(bool visible) override;
  void ShowUpdateIcon(mojom::UpdateSeverity severity,
                      bool factory_reset_required,
                      mojom::UpdateType update_type) override;
  void SetUpdateOverCellularAvailableIconVisible(bool visible) override;

  ClockModel* clock() { return clock_.get(); }
  EnterpriseDomainModel* enterprise_domain() {
    return enterprise_domain_.get();
  }
  TracingModel* tracing() { return tracing_.get(); }
  UpdateModel* update_model() { return update_model_.get(); }

 private:
  std::unique_ptr<ClockModel> clock_;
  std::unique_ptr<EnterpriseDomainModel> enterprise_domain_;
  std::unique_ptr<TracingModel> tracing_;
  std::unique_ptr<UpdateModel> update_model_;

  // TODO(tetsui): Add following as a sub-model of SystemTrayModel:
  // * BluetoothModel

  DISALLOW_COPY_AND_ASSIGN(SystemTrayModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
