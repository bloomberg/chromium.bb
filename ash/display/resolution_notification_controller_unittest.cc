// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/resolution_notification_controller.h"

#include "ash/display/display_manager.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace internal {

class ResolutionNotificationControllerTest : public ash::test::AshTestBase {
 public:
  ResolutionNotificationControllerTest()
      : accept_count_(0) {
  }

  virtual ~ResolutionNotificationControllerTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();
    ResolutionNotificationController::SuppressTimerForTest();
  }

  void SetDisplayResolutionAndNotify(const gfx::Display& display,
                                     const gfx::Size& new_resolution) {
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();
    const DisplayInfo& info = display_manager->GetDisplayInfo(display.id());
    Shell::GetInstance()->resolution_notification_controller()->
        SetDisplayResolutionAndNotify(
            display.id(),
            info.size_in_pixel(),
            new_resolution,
            base::Bind(&ResolutionNotificationControllerTest::OnAccepted,
                       base::Unretained(this)));

    // OnConfigurationChanged event won't be emitted in the test environment,
    // so invoke UpdateDisplay() to emit that event explicitly.
    std::vector<DisplayInfo> info_list;
    for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
      int64 id = display_manager->GetDisplayAt(i).id();
      DisplayInfo info = display_manager->GetDisplayInfo(id);
      if (display.id() == id) {
        gfx::Rect bounds = info.bounds_in_native();
        bounds.set_size(new_resolution);
        info.SetBounds(bounds);
      }
      info_list.push_back(info);
    }
    display_manager->OnNativeDisplaysChanged(info_list);
    RunAllPendingInMessageLoop();
  }

  void ClickOnNotification() {
    message_center::MessageCenter::Get()->ClickOnNotification(
        ResolutionNotificationController::kNotificationId);
  }

  void ClickOnNotificationButton(int index) {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        ResolutionNotificationController::kNotificationId, index);
  }

  void CloseNotification() {
    message_center::MessageCenter::Get()->RemoveNotification(
        ResolutionNotificationController::kNotificationId, true /* by_user */);
  }

  bool IsNotificationVisible() {
    return message_center::MessageCenter::Get()->HasNotification(
        ResolutionNotificationController::kNotificationId);
  }

  void TickTimer() {
    controller()->OnTimerTick();
  }

  ResolutionNotificationController* controller() {
    return Shell::GetInstance()->resolution_notification_controller();
  }

  int accept_count() const {
    return accept_count_;
  }

 private:
  void OnAccepted() {
    EXPECT_FALSE(controller()->DoesNotificationTimeout());
    accept_count_++;
  }

  int accept_count_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionNotificationControllerTest);
};

// Basic behaviors and verifies it doesn't cause crashes.
TEST_F(ResolutionNotificationControllerTest, Basic) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300#300x300|200x200,250x250#250x250|200x200");
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(
      ScreenAsh::GetSecondaryDisplay(), gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(controller()->DoesNotificationTimeout());
  gfx::Size resolution;
  EXPECT_TRUE(
      display_manager->GetSelectedResolutionForDisplayId(id2, &resolution));
  EXPECT_EQ("200x200", resolution.ToString());

  // Click the revert button, which reverts to the best resolution.
  ClickOnNotificationButton(0);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  EXPECT_FALSE(
      display_manager->GetSelectedResolutionForDisplayId(id2, &resolution));
}

TEST_F(ResolutionNotificationControllerTest, ClickMeansAccept) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300#300x300|200x200,250x250#250x250|200x200");
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(
      ScreenAsh::GetSecondaryDisplay(), gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(controller()->DoesNotificationTimeout());
  gfx::Size resolution;
  EXPECT_TRUE(
      display_manager->GetSelectedResolutionForDisplayId(id2, &resolution));
  EXPECT_EQ("200x200", resolution.ToString());

  // Click the revert button, which reverts the resolution.
  ClickOnNotification();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
  EXPECT_TRUE(
      display_manager->GetSelectedResolutionForDisplayId(id2, &resolution));
  EXPECT_EQ("200x200", resolution.ToString());
}

TEST_F(ResolutionNotificationControllerTest, AcceptButton) {
  if (!SupportsMultipleDisplays())
    return;

  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("300x300#300x300|200x200");
  const gfx::Display& display = ash::Shell::GetScreen()->GetPrimaryDisplay();
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());

  // If there's a single display only, it will have timeout and the first button
  // becomes accept.
  EXPECT_TRUE(controller()->DoesNotificationTimeout());
  ClickOnNotificationButton(0);
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
  gfx::Size resolution;
  EXPECT_TRUE(display_manager->GetSelectedResolutionForDisplayId(
      display.id(), &resolution));
  EXPECT_EQ("200x200", resolution.ToString());

  // In that case the second button is revert.
  UpdateDisplay("300x300#300x300|200x200");
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());

  EXPECT_TRUE(controller()->DoesNotificationTimeout());
  ClickOnNotificationButton(1);
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
  EXPECT_FALSE(display_manager->GetSelectedResolutionForDisplayId(
      display.id(), &resolution));
}

TEST_F(ResolutionNotificationControllerTest, Close) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("100x100,150x150#150x150|200x200");
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(
      ScreenAsh::GetSecondaryDisplay(), gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(controller()->DoesNotificationTimeout());
  gfx::Size resolution;
  EXPECT_TRUE(
      display_manager->GetSelectedResolutionForDisplayId(id2, &resolution));
  EXPECT_EQ("200x200", resolution.ToString());

  // Close the notification (imitates clicking [x] button). Also verifies if
  // this does not cause a crash.  See crbug.com/271784
  CloseNotification();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
}

TEST_F(ResolutionNotificationControllerTest, Timeout) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300#300x300|200x200");
  const gfx::Display& display = ash::Shell::GetScreen()->GetPrimaryDisplay();
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200));

  for (int i = 0; i < ResolutionNotificationController::kTimeoutInSec; ++i) {
    EXPECT_TRUE(IsNotificationVisible()) << "notification is closed after "
                                         << i << "-th timer tick";
    TickTimer();
    RunAllPendingInMessageLoop();
  }
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  gfx::Size resolution;
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  EXPECT_FALSE(display_manager->GetSelectedResolutionForDisplayId(
      display.id(), &resolution));
}

TEST_F(ResolutionNotificationControllerTest, DisplayDisconnected) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300#300x300|200x200,200x200#250x250|200x200|100x100");
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  SetDisplayResolutionAndNotify(
      ScreenAsh::GetSecondaryDisplay(), gfx::Size(100, 100));
  ASSERT_TRUE(IsNotificationVisible());

  // Disconnects the secondary display and verifies it doesn't cause crashes.
  UpdateDisplay("300x300#300x300|200x200");
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  gfx::Size resolution;
  EXPECT_TRUE(
      display_manager->GetSelectedResolutionForDisplayId(id2, &resolution));
  EXPECT_EQ("200x200", resolution.ToString());
}

TEST_F(ResolutionNotificationControllerTest, MultipleResolutionChange) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300#300x300|200x200,250x250#250x250|200x200");
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  SetDisplayResolutionAndNotify(
      ScreenAsh::GetSecondaryDisplay(), gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(controller()->DoesNotificationTimeout());
  gfx::Size resolution;
  EXPECT_TRUE(
      display_manager->GetSelectedResolutionForDisplayId(id2, &resolution));
  EXPECT_EQ("200x200", resolution.ToString());

  // Invokes SetDisplayResolutionAndNotify during the previous notification is
  // visible.
  SetDisplayResolutionAndNotify(
      ScreenAsh::GetSecondaryDisplay(), gfx::Size(250, 250));
  EXPECT_FALSE(
      display_manager->GetSelectedResolutionForDisplayId(id2, &resolution));

  // Then, click the revert button. Although |old_resolution| for the second
  // SetDisplayResolutionAndNotify is 200x200, it should revert to the original
  // size 150x150.
  ClickOnNotificationButton(0);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  EXPECT_FALSE(
      display_manager->GetSelectedResolutionForDisplayId(id2, &resolution));
}

}  // namespace internal
}  // namespace ash
