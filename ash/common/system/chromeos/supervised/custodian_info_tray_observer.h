// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_SUPERVISED_CUSTODIAN_INFO_TRAY_OBSERVER_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_SUPERVISED_CUSTODIAN_INFO_TRAY_OBSERVER_H_

namespace ash {

// Used to observe SystemTrayDelegate.
class CustodianInfoTrayObserver {
 public:
  // Called when information about the supervised user's custodian is changed,
  // e.g. the display name.
  virtual void OnCustodianInfoChanged() = 0;

 protected:
  virtual ~CustodianInfoTrayObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_SUPERVISED_CUSTODIAN_INFO_TRAY_OBSERVER_H_
