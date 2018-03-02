// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_

#include <memory>

#include "base/macros.h"

namespace ash {

class EnterpriseDomainModel;

// Top level model of SystemTray.
class SystemTrayModel {
 public:
  SystemTrayModel();
  ~SystemTrayModel();

  EnterpriseDomainModel* enterprise_domain() {
    return enterprise_domain_.get();
  }

 private:
  std::unique_ptr<EnterpriseDomainModel> enterprise_domain_;

  // TODO(tetsui): Add following as a sub-model of SystemTrayModel:
  // * BluetoothModel
  // * ClockModel

  DISALLOW_COPY_AND_ASSIGN(SystemTrayModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
