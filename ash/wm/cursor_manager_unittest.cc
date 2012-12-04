// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/cursor_manager.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/wm/image_cursors.h"

namespace ash {
namespace test {

typedef test::AshTestBase CursorManagerTest;

TEST_F(CursorManagerTest, LockCursor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());
  cursor_manager->SetDeviceScaleFactor(2.0f);
  EXPECT_EQ(2.0f, test_api.GetDeviceScaleFactor());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());

  cursor_manager->LockCursor();
  EXPECT_TRUE(cursor_manager->is_cursor_locked());

  // Cursor type does not change while cursor is locked.
  cursor_manager->SetCursor(ui::kCursorPointer);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  // Device scale factor does change even while cursor is locked.
  cursor_manager->SetDeviceScaleFactor(1.0f);
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());

  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->is_cursor_locked());

  // Cursor type changes to the one specified while cursor is locked.
  EXPECT_EQ(ui::kCursorPointer, test_api.GetCurrentCursor().native_type());
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(CursorManagerTest, SetCursor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
  cursor_manager->SetCursor(ui::kCursorPointer);
  EXPECT_EQ(ui::kCursorPointer, test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(CursorManagerTest, ShowCursor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  cursor_manager->ShowCursor(true);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  cursor_manager->ShowCursor(false);
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  // Check if cursor visibility is locked.
  cursor_manager->LockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  cursor_manager->ShowCursor(true);
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  cursor_manager->LockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  cursor_manager->ShowCursor(false);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Checks setting visiblity while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager->LockCursor();
  cursor_manager->ShowCursor(false);
  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  cursor_manager->ShowCursor(true);
  cursor_manager->LockCursor();
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  cursor_manager->LockCursor();
  cursor_manager->ShowCursor(true);
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  cursor_manager->ShowCursor(false);
  cursor_manager->LockCursor();
  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
}

TEST_F(CursorManagerTest, SetDeviceScaleFactor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetDeviceScaleFactor(2.0f);
  EXPECT_EQ(2.0f, test_api.GetDeviceScaleFactor());
  cursor_manager->SetDeviceScaleFactor(1.0f);
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());
}

}  // namespace test
}  // namespace ash
