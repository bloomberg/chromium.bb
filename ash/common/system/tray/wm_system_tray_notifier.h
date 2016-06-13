// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_WM_SYSTEM_TRAY_NOTIFIER_H_
#define ASH_COMMON_SYSTEM_TRAY_WM_SYSTEM_TRAY_NOTIFIER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

struct UpdateInfo;
class UpdateObserver;

// Observer and notification manager for the ash system tray. This version
// contains the observers that have been ported to //ash/common. See also
// ash::SystemTrayNotifier.
// TODO(jamescook): After all the observers have been moved, rename this class
// to SystemTrayNotifier.
class ASH_EXPORT WmSystemTrayNotifier {
 public:
  WmSystemTrayNotifier();
  ~WmSystemTrayNotifier();

  void AddUpdateObserver(UpdateObserver* observer);
  void RemoveUpdateObserver(UpdateObserver* observer);

  void NotifyUpdateRecommended(const UpdateInfo& info);

 private:
  base::ObserverList<UpdateObserver> update_observers_;

  DISALLOW_COPY_AND_ASSIGN(WmSystemTrayNotifier);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_WM_SYSTEM_TRAY_NOTIFIER_H_
