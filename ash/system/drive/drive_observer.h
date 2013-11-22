// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DRIVE_DRIVE_OBSERVER_H_
#define ASH_SYSTEM_DRIVE_DRIVE_OBSERVER_H_

#include "ash/system/tray/system_tray_delegate.h"

namespace ash {

class DriveObserver {
 public:
  virtual void OnDriveJobUpdated(const DriveOperationStatus& status) = 0;

 protected:
  virtual ~DriveObserver() {}
};

}  // namespace ash

#endif  // ASH_SYSTEM_DRIVE_DRIVE_OBSERVER_H_
