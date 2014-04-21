// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_event_observer.h"

#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {

class PowerEventObserverTest : public test::AshTestBase {
 public:
  PowerEventObserverTest() {}
  virtual ~PowerEventObserverTest() {}

  // test::AshTestBase::SetUp() overrides:
  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    observer_.reset(new PowerEventObserver());
  }

  virtual void TearDown() OVERRIDE {
    observer_.reset();
    test::AshTestBase::TearDown();
  }

 protected:
  scoped_ptr<PowerEventObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerEventObserverTest);
};

TEST_F(PowerEventObserverTest, LockBeforeSuspend) {
  chromeos::PowerManagerClient* client =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  ASSERT_EQ(0, client->GetNumPendingSuspendReadinessCallbacks());

  // Check that the observer requests a suspend-readiness callback when it hears
  // that the system is about to suspend.
  SetCanLockScreen(true);
  SetShouldLockScreenBeforeSuspending(true);
  observer_->SuspendImminent();
  EXPECT_EQ(1, client->GetNumPendingSuspendReadinessCallbacks());

  // It should run the callback when it hears that the screen is locked.
  observer_->ScreenIsLocked();
  RunAllPendingInMessageLoop();
  EXPECT_EQ(0, client->GetNumPendingSuspendReadinessCallbacks());

  // If the system is already locked, no callback should be requested.
  observer_->SuspendDone(base::TimeDelta());
  observer_->ScreenIsUnlocked();
  observer_->ScreenIsLocked();
  observer_->SuspendImminent();
  EXPECT_EQ(0, client->GetNumPendingSuspendReadinessCallbacks());

  // It also shouldn't request a callback if it isn't instructed to lock the
  // screen.
  observer_->SuspendDone(base::TimeDelta());
  SetShouldLockScreenBeforeSuspending(false);
  observer_->SuspendImminent();
  EXPECT_EQ(0, client->GetNumPendingSuspendReadinessCallbacks());
}

}  // namespace ash
