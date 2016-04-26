// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_locking_manager.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shelf_test_api.h"

namespace ash {
namespace test {

// Tests the shelf behavior when the screen or session is locked.
class ShelfLockingManagerTest : public ash::test::AshTestBase {
 public:
  ShelfLockingManagerTest() {}

  ShelfLockingManager* GetShelfLockingManager() {
    return ShelfTestAPI(Shelf::ForPrimaryDisplay()).shelf_locking_manager();
  }

  void SetScreenLocked(bool locked) {
    GetShelfLockingManager()->OnLockStateChanged(locked);
  }

  void SetSessionState(SessionStateDelegate::SessionState state) {
    GetShelfLockingManager()->SessionStateChanged(state);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfLockingManagerTest);
};

// Makes sure shelf alignment is correct for lock screen.
TEST_F(ShelfLockingManagerTest, AlignmentLockedWhileScreenLocked) {
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  EXPECT_EQ(wm::SHELF_ALIGNMENT_BOTTOM, shelf->alignment());

  shelf->SetAlignment(wm::SHELF_ALIGNMENT_LEFT);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_LEFT, shelf->alignment());

  SetScreenLocked(true);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->alignment());
  SetScreenLocked(false);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_LEFT, shelf->alignment());
}

// Makes sure shelf alignment is correct for login and add user screens.
TEST_F(ShelfLockingManagerTest, AlignmentLockedWhileSessionLocked) {
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  EXPECT_EQ(wm::SHELF_ALIGNMENT_BOTTOM, shelf->alignment());

  shelf->SetAlignment(wm::SHELF_ALIGNMENT_RIGHT);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_RIGHT, shelf->alignment());

  SetSessionState(SessionStateDelegate::SESSION_STATE_LOGIN_PRIMARY);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->alignment());
  SetSessionState(SessionStateDelegate::SESSION_STATE_ACTIVE);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_RIGHT, shelf->alignment());

  SetSessionState(SessionStateDelegate::SESSION_STATE_LOGIN_SECONDARY);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->alignment());
  SetSessionState(SessionStateDelegate::SESSION_STATE_ACTIVE);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_RIGHT, shelf->alignment());
}

// Makes sure shelf alignment changes are stored, not set, while locked.
TEST_F(ShelfLockingManagerTest, AlignmentChangesDeferredWhileLocked) {
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  EXPECT_EQ(wm::SHELF_ALIGNMENT_BOTTOM, shelf->alignment());

  SetScreenLocked(true);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->alignment());
  shelf->SetAlignment(wm::SHELF_ALIGNMENT_RIGHT);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->alignment());
  SetScreenLocked(false);
  EXPECT_EQ(wm::SHELF_ALIGNMENT_RIGHT, shelf->alignment());
}

}  // namespace test
}  // namespace ash
