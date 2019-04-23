// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/user_chooser_detailed_view_controller.h"

#include "ash/system/unified/user_chooser_view.h"

namespace ash {

UserChooserDetailedViewController::UserChooserDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

UserChooserDetailedViewController::~UserChooserDetailedViewController() =
    default;

views::View* UserChooserDetailedViewController::CreateView() {
  return new UserChooserView(tray_controller_);
}

}  // namespace ash
