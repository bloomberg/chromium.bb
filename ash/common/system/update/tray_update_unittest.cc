// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/update/tray_update.h"

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_system_tray_delegate.h"
#include "base/macros.h"

namespace ash {

using TrayUpdateTest = test::AshTestBase;

// Tests that the update icon becomes visible when an update becomes
// available.
TEST_F(TrayUpdateTest, VisibilityAfterUpdate) {
  TrayUpdate* tray_update = GetPrimarySystemTray()->GetTrayUpdateForTesting();

  // The system starts with no update pending.
  test::TestSystemTrayDelegate* tray_delegate = GetSystemTrayDelegate();
  UpdateInfo initial_info;
  tray_delegate->GetSystemUpdateInfo(&initial_info);
  EXPECT_FALSE(initial_info.update_required);

  // When no update is pending, the item isn't visible.
  EXPECT_FALSE(tray_update->tray_view()->visible());

  // Simulate an update.
  UpdateInfo info;
  info.update_required = true;
  WmShell::Get()->system_tray_notifier()->NotifyUpdateRecommended(info);

  // Tray item is now visible.
  EXPECT_TRUE(tray_update->tray_view()->visible());
}

}  // namespace ash
