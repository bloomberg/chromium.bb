// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_event_observer.h"

#include <memory>

#include "ash/common/test/test_session_state_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {

class PowerEventObserverTest : public test::AshTestBase {
 public:
  PowerEventObserverTest() {}
  ~PowerEventObserverTest() override {}

  // test::AshTestBase::SetUp() overrides:
  void SetUp() override {
    test::AshTestBase::SetUp();
    observer_.reset(new PowerEventObserver());
  }

  void TearDown() override {
    observer_.reset();
    test::AshTestBase::TearDown();
  }

 protected:
  int GetNumVisibleCompositors() {
    int result = 0;
    for (auto* window : Shell::GetAllRootWindows()) {
      if (window->GetHost()->compositor()->IsVisible())
        ++result;
    }

    return result;
  }

  std::unique_ptr<PowerEventObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerEventObserverTest);
};

TEST_F(PowerEventObserverTest, LockBeforeSuspend) {
  chromeos::PowerManagerClient* client =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  ASSERT_EQ(0, client->GetNumPendingSuspendReadinessCallbacks());

  // Check that the observer requests a suspend-readiness callback when it hears
  // that the system is about to suspend.
  test::TestSessionStateDelegate::SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);
  observer_->SuspendImminent();
  EXPECT_EQ(1, client->GetNumPendingSuspendReadinessCallbacks());

  // It should run the callback when it hears that the screen is locked and the
  // lock screen animations have completed.
  observer_->ScreenIsLocked();
  observer_->OnLockAnimationsComplete();
  EXPECT_EQ(0, client->GetNumPendingSuspendReadinessCallbacks());

  // If the system is already locked, no callback should be requested.
  observer_->SuspendDone(base::TimeDelta());
  observer_->ScreenIsUnlocked();
  observer_->ScreenIsLocked();
  observer_->OnLockAnimationsComplete();
  observer_->SuspendImminent();
  EXPECT_EQ(0, client->GetNumPendingSuspendReadinessCallbacks());

  // It also shouldn't request a callback if it isn't instructed to lock the
  // screen.
  observer_->SuspendDone(base::TimeDelta());
  SetShouldLockScreenAutomatically(false);
  observer_->SuspendImminent();
  EXPECT_EQ(0, client->GetNumPendingSuspendReadinessCallbacks());
}

TEST_F(PowerEventObserverTest, SetInvisibleBeforeSuspend) {
  // Tests that all the Compositors are marked invisible before a suspend
  // request when the screen is not supposed to be locked before a suspend.
  EXPECT_EQ(1, GetNumVisibleCompositors());

  observer_->SuspendImminent();
  EXPECT_EQ(0, GetNumVisibleCompositors());
  observer_->SuspendDone(base::TimeDelta());

  // Tests that all the Compositors are marked invisible _after_ the screen lock
  // animations have completed.
  test::TestSessionStateDelegate::SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  observer_->SuspendImminent();
  EXPECT_EQ(1, GetNumVisibleCompositors());

  observer_->ScreenIsLocked();
  EXPECT_EQ(1, GetNumVisibleCompositors());

  observer_->OnLockAnimationsComplete();
  EXPECT_EQ(0, GetNumVisibleCompositors());

  observer_->SuspendDone(base::TimeDelta());
  EXPECT_EQ(1, GetNumVisibleCompositors());
}

TEST_F(PowerEventObserverTest, CanceledSuspend) {
  // Tests that the Compositors are not marked invisible if a suspend is
  // canceled or the system resumes before the lock screen is ready.
  test::TestSessionStateDelegate::SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);
  observer_->SuspendImminent();
  EXPECT_EQ(1, GetNumVisibleCompositors());

  observer_->SuspendDone(base::TimeDelta());
  observer_->ScreenIsLocked();
  observer_->OnLockAnimationsComplete();
  EXPECT_EQ(1, GetNumVisibleCompositors());
}

TEST_F(PowerEventObserverTest, DelayResuspendForLockAnimations) {
  // Tests that the following order of events is handled correctly:
  //
  // - A suspend request is started.
  // - The screen is locked.
  // - The suspend request is canceled.
  // - Another suspend request is started.
  // - The screen lock animations complete.
  //
  // In this case, the observer should block the second suspend request until
  // the animations have completed.
  test::TestSessionStateDelegate::SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  chromeos::PowerManagerClient* client =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  observer_->SuspendImminent();
  EXPECT_EQ(1, client->GetNumPendingSuspendReadinessCallbacks());

  observer_->ScreenIsLocked();
  observer_->SuspendDone(base::TimeDelta());
  observer_->SuspendImminent();

  // The expected number of suspend readiness callbacks is 2 because the
  // observer has not run the callback that it got from the first suspend
  // request.  The real PowerManagerClient would reset its internal counter in
  // this situation but the stub client is not that smart.
  EXPECT_EQ(2, client->GetNumPendingSuspendReadinessCallbacks());

  observer_->OnLockAnimationsComplete();
  EXPECT_EQ(1, client->GetNumPendingSuspendReadinessCallbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

}  // namespace ash
