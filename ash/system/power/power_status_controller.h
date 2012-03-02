// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_STATUS_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_STATUS_CONTROLLER_H_

namespace ash {

struct PowerSupplyStatus;

class PowerStatusController {
 public:
  virtual ~PowerStatusController() {}

  virtual void OnPowerStatusChanged(const PowerSupplyStatus& status) = 0;
};

};

#endif  // ASH_SYSTEM_POWER_POWER_STATUS_CONTROLLER_H_
