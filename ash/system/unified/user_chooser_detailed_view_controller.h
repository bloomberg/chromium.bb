// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_USER_CHOOSER_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_USER_CHOOSER_DETAILED_VIEW_CONTROLLER_H_

#include "ash/system/unified/detailed_view_controller.h"
#include "base/macros.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of the user chooser detailed view (used for multi-user sign-in) in
// UnifiedSystemTray.
class UserChooserDetailedViewController : public DetailedViewController {
 public:
  explicit UserChooserDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  ~UserChooserDetailedViewController() override;

  // DetailedViewController:
  views::View* CreateView() override;

 private:
  UnifiedSystemTrayController* tray_controller_;

  DISALLOW_COPY_AND_ASSIGN(UserChooserDetailedViewController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_USER_CHOOSER_DETAILED_VIEW_CONTROLLER_H_
