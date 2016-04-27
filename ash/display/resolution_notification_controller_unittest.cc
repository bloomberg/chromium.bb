// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/resolution_notification_controller.h"

#include "ash/display/display_manager.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"

namespace ash {
namespace {

base::string16 ExpectedNotificationMessage(int64_t display_id,
                                           const gfx::Size& new_resolution) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
      base::UTF8ToUTF16(
          Shell::GetInstance()->display_manager()->GetDisplayNameForId(
              display_id)),
      base::UTF8ToUTF16(new_resolution.ToString()));
}

base::string16 ExpectedFallbackNotificationMessage(
    int64_t display_id,
    const gfx::Size& specified_resolution,
    const gfx::Size& fallback_resolution) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED_TO_UNSUPPORTED,
      base::UTF8ToUTF16(
          Shell::GetInstance()->display_manager()->GetDisplayNameForId(
              display_id)),
      base::UTF8ToUTF16(specified_resolution.ToString()),
      base::UTF8ToUTF16(fallback_resolution.ToString()));
}

}  // namespace

class ResolutionNotificationControllerTest : public ash::test::AshTestBase {
 public:
  ResolutionNotificationControllerTest()
      : accept_count_(0) {
  }

  ~ResolutionNotificationControllerTest() override {}

 protected:
  void SetUp() override {
    ash::test::AshTestBase::SetUp();
    ResolutionNotificationController::SuppressTimerForTest();
  }

  void SetDisplayResolutionAndNotifyWithResolution(
      const display::Display& display,
      const gfx::Size& new_resolution,
      const gfx::Size& actual_new_resolution) {
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();

    const DisplayInfo& info = display_manager->GetDisplayInfo(display.id());
    DisplayMode old_mode(info.size_in_pixel(),
                         60 /* refresh_rate */,
                         false /* interlaced */,
                         false /* native */);
    DisplayMode new_mode = old_mode;
    new_mode.size = new_resolution;

    if (display_manager->SetDisplayMode(display.id(), new_mode)) {
      controller()->PrepareNotification(
          display.id(),
          old_mode,
          new_mode,
          base::Bind(&ResolutionNotificationControllerTest::OnAccepted,
                     base::Unretained(this)));
    }

    // OnConfigurationChanged event won't be emitted in the test environment,
    // so invoke UpdateDisplay() to emit that event explicitly.
    std::vector<DisplayInfo> info_list;
    for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
      int64_t id = display_manager->GetDisplayAt(i).id();
      DisplayInfo info = display_manager->GetDisplayInfo(id);
      if (display.id() == id) {
        gfx::Rect bounds = info.bounds_in_native();
        bounds.set_size(actual_new_resolution);
        info.SetBounds(bounds);
      }
      info_list.push_back(info);
    }
    display_manager->OnNativeDisplaysChanged(info_list);
    RunAllPendingInMessageLoop();
  }

  void SetDisplayResolutionAndNotify(const display::Display& display,
                                     const gfx::Size& new_resolution) {
    SetDisplayResolutionAndNotifyWithResolution(
        display, new_resolution, new_resolution);
  }

  static base::string16 GetNotificationMessage() {
    const message_center::NotificationList::Notifications& notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (message_center::NotificationList::Notifications::const_iterator iter =
             notifications.begin(); iter != notifications.end(); ++iter) {
      if ((*iter)->id() == ResolutionNotificationController::kNotificationId)
        return (*iter)->title();
    }

    return base::string16();
  }

  static void ClickOnNotification() {
    message_center::MessageCenter::Get()->ClickOnNotification(
        ResolutionNotificationController::kNotificationId);
  }

  static void ClickOnNotificationButton(int index) {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        ResolutionNotificationController::kNotificationId, index);
  }

  static void CloseNotification() {
    message_center::MessageCenter::Get()->RemoveNotification(
        ResolutionNotificationController::kNotificationId, true /* by_user */);
  }

  static bool IsNotificationVisible() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        ResolutionNotificationController::kNotificationId);
  }

  static void TickTimer() {
    controller()->OnTimerTick();
  }

  static ResolutionNotificationController* controller() {
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

  UpdateDisplay("300x300#300x300%57|200x200%58,250x250#250x250%59|200x200%60");
  int64_t id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(
      ScreenUtil::GetSecondaryDisplay(), gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(controller()->DoesNotificationTimeout());
  EXPECT_EQ(ExpectedNotificationMessage(id2, gfx::Size(200, 200)),
            GetNotificationMessage());
  DisplayMode mode;
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size.ToString());
  EXPECT_EQ(60.0, mode.refresh_rate);

  // Click the revert button, which reverts to the best resolution.
  ClickOnNotificationButton(0);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("250x250", mode.size.ToString());
  EXPECT_EQ(59.0, mode.refresh_rate);
}

TEST_F(ResolutionNotificationControllerTest, ClickMeansAccept) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300#300x300%57|200x200%58,250x250#250x250%59|200x200%60");
  int64_t id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(
      ScreenUtil::GetSecondaryDisplay(), gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(controller()->DoesNotificationTimeout());
  DisplayMode mode;
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size.ToString());
  EXPECT_EQ(60.0, mode.refresh_rate);

  // Click the revert button, which reverts the resolution.
  ClickOnNotification();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size.ToString());
  EXPECT_EQ(60.0, mode.refresh_rate);
}

TEST_F(ResolutionNotificationControllerTest, AcceptButton) {
  if (!SupportsMultipleDisplays())
    return;

  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("300x300#300x300%59|200x200%60");
  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());

  // If there's a single display only, it will have timeout and the first button
  // becomes accept.
  EXPECT_TRUE(controller()->DoesNotificationTimeout());
  ClickOnNotificationButton(0);
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
  DisplayMode mode;
  EXPECT_TRUE(
      display_manager->GetSelectedModeForDisplayId(display.id(), &mode));
  EXPECT_EQ("200x200", mode.size.ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate);

  // In that case the second button is revert.
  UpdateDisplay("300x300#300x300%59|200x200%60");
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());

  EXPECT_TRUE(controller()->DoesNotificationTimeout());
  ClickOnNotificationButton(1);
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
  EXPECT_TRUE(
      display_manager->GetSelectedModeForDisplayId(display.id(), &mode));
  EXPECT_EQ("300x300", mode.size.ToString());
  EXPECT_EQ(59.0f, mode.refresh_rate);
}

TEST_F(ResolutionNotificationControllerTest, Close) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("100x100,150x150#150x150%59|200x200%60");
  int64_t id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(
      ScreenUtil::GetSecondaryDisplay(), gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(controller()->DoesNotificationTimeout());
  DisplayMode mode;
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size.ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate);

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

  UpdateDisplay("300x300#300x300%59|200x200%60");
  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200));

  for (int i = 0; i < ResolutionNotificationController::kTimeoutInSec; ++i) {
    EXPECT_TRUE(IsNotificationVisible()) << "notification is closed after "
                                         << i << "-th timer tick";
    TickTimer();
    RunAllPendingInMessageLoop();
  }
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  DisplayMode mode;
  EXPECT_TRUE(
      display_manager->GetSelectedModeForDisplayId(display.id(), &mode));
  EXPECT_EQ("300x300", mode.size.ToString());
  EXPECT_EQ(59.0f, mode.refresh_rate);
}

TEST_F(ResolutionNotificationControllerTest, DisplayDisconnected) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300#300x300%56|200x200%57,"
                "200x200#250x250%58|200x200%59|100x100%60");
  int64_t id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  SetDisplayResolutionAndNotify(
      ScreenUtil::GetSecondaryDisplay(), gfx::Size(100, 100));
  ASSERT_TRUE(IsNotificationVisible());

  // Disconnects the secondary display and verifies it doesn't cause crashes.
  UpdateDisplay("300x300#300x300%56|200x200%57");
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  DisplayMode mode;
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  gfx::Size resolution;
  EXPECT_EQ("200x200", mode.size.ToString());
  EXPECT_EQ(59.0f, mode.refresh_rate);
}

TEST_F(ResolutionNotificationControllerTest, MultipleResolutionChange) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300#300x300%56|200x200%57,"
                "250x250#250x250%58|200x200%59");
  int64_t id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  SetDisplayResolutionAndNotify(
      ScreenUtil::GetSecondaryDisplay(), gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(controller()->DoesNotificationTimeout());
  DisplayMode mode;
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size.ToString());
  EXPECT_EQ(59.0f, mode.refresh_rate);

  // Invokes SetDisplayResolutionAndNotify during the previous notification is
  // visible.
  SetDisplayResolutionAndNotify(
      ScreenUtil::GetSecondaryDisplay(), gfx::Size(250, 250));
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("250x250", mode.size.ToString());
  EXPECT_EQ(58.0f, mode.refresh_rate);

  // Then, click the revert button. Although |old_resolution| for the second
  // SetDisplayResolutionAndNotify is 200x200, it should revert to the original
  // size 250x250.
  ClickOnNotificationButton(0);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("250x250", mode.size.ToString());
  EXPECT_EQ(58.0f, mode.refresh_rate);
}

TEST_F(ResolutionNotificationControllerTest, Fallback) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300#300x300%56|200x200%57,"
                "250x250#250x250%58|220x220%59|200x200%60");
  int64_t id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotifyWithResolution(
      ScreenUtil::GetSecondaryDisplay(),
      gfx::Size(220, 220),
      gfx::Size(200, 200));
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(controller()->DoesNotificationTimeout());
  EXPECT_EQ(
      ExpectedFallbackNotificationMessage(
          id2, gfx::Size(220, 220), gfx::Size(200, 200)),
      GetNotificationMessage());
  DisplayMode mode;
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size.ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate);

  // Click the revert button, which reverts to the best resolution.
  ClickOnNotificationButton(0);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  EXPECT_TRUE(display_manager->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("250x250", mode.size.ToString());
  EXPECT_EQ(58.0f, mode.refresh_rate);
}

}  // namespace ash
