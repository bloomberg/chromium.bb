// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_locking_manager.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace test {

// Tests the shelf behavior when the screen or session is locked.
class ShelfLockingManagerTest : public ash::test::AshTestBase {
 public:
  ShelfLockingManagerTest() {}

  ShelfLockingManager* GetShelfLockingManager() {
    return GetPrimaryShelf()->GetShelfLockingManagerForTesting();
  }

  void SetScreenLocked(bool locked) {
    GetShelfLockingManager()->OnLockStateChanged(locked);
  }

  void SetSessionState(session_manager::SessionState state) {
    GetShelfLockingManager()->SessionStateChanged(state);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfLockingManagerTest);
};

// Makes sure shelf alignment is correct for lock screen.
TEST_F(ShelfLockingManagerTest, AlignmentLockedWhileScreenLocked) {
  WmShelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->GetAlignment());

  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf->GetAlignment());

  SetScreenLocked(true);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->GetAlignment());
  SetScreenLocked(false);
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf->GetAlignment());
}

// Makes sure shelf alignment is correct for login and add user screens.
TEST_F(ShelfLockingManagerTest, AlignmentLockedWhileSessionLocked) {
  WmShelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->GetAlignment());

  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, shelf->GetAlignment());

  SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->GetAlignment());
  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, shelf->GetAlignment());

  SetSessionState(session_manager::SessionState::LOGIN_SECONDARY);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->GetAlignment());
  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, shelf->GetAlignment());
}

// Makes sure shelf alignment changes are stored, not set, while locked.
TEST_F(ShelfLockingManagerTest, AlignmentChangesDeferredWhileLocked) {
  WmShelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->GetAlignment());

  SetScreenLocked(true);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->GetAlignment());
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->GetAlignment());
  SetScreenLocked(false);
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, shelf->GetAlignment());
}

}  // namespace test
}  // namespace ash
