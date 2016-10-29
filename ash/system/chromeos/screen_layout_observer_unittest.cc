// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/screen_layout_observer.h"

#include "ash/common/system/chromeos/devicetype_utils.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/views/controls/label.h"

namespace ash {

class ScreenLayoutObserverTest : public test::AshTestBase {
 public:
  ScreenLayoutObserverTest();
  ~ScreenLayoutObserverTest() override;

 protected:
  ScreenLayoutObserver* GetScreenLayoutObserver();
  void CheckUpdate();

  void CloseNotification();
  base::string16 GetDisplayNotificationText() const;
  base::string16 GetDisplayNotificationAdditionalText() const;

  base::string16 GetFirstDisplayName();

  base::string16 GetSecondDisplayName();

  base::string16 GetMirroringDisplayName();

 private:
  const message_center::Notification* GetDisplayNotification() const;

  DISALLOW_COPY_AND_ASSIGN(ScreenLayoutObserverTest);
};

ScreenLayoutObserverTest::ScreenLayoutObserverTest() {}

ScreenLayoutObserverTest::~ScreenLayoutObserverTest() {}

ScreenLayoutObserver* ScreenLayoutObserverTest::GetScreenLayoutObserver() {
  return Shell::GetInstance()->screen_layout_observer();
}

void ScreenLayoutObserverTest::CloseNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(
      ScreenLayoutObserver::kNotificationId, false);
  RunAllPendingInMessageLoop();
}

base::string16 ScreenLayoutObserverTest::GetDisplayNotificationText() const {
  const message_center::Notification* notification = GetDisplayNotification();
  return notification ? notification->title() : base::string16();
}

base::string16 ScreenLayoutObserverTest::GetDisplayNotificationAdditionalText()
    const {
  const message_center::Notification* notification = GetDisplayNotification();
  return notification ? notification->message() : base::string16();
}

base::string16 ScreenLayoutObserverTest::GetFirstDisplayName() {
  return base::UTF8ToUTF16(display_manager()->GetDisplayNameForId(
      display_manager()->first_display_id()));
}

base::string16 ScreenLayoutObserverTest::GetSecondDisplayName() {
  return base::UTF8ToUTF16(display_manager()->GetDisplayNameForId(
      display_manager()->GetSecondaryDisplay().id()));
}

base::string16 ScreenLayoutObserverTest::GetMirroringDisplayName() {
  return base::UTF8ToUTF16(display_manager()->GetDisplayNameForId(
      display_manager()->mirroring_display_id()));
}

const message_center::Notification*
ScreenLayoutObserverTest::GetDisplayNotification() const {
  const message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  for (message_center::NotificationList::Notifications::const_iterator iter =
           notifications.begin();
       iter != notifications.end(); ++iter) {
    if ((*iter)->id() == ScreenLayoutObserver::kNotificationId)
      return *iter;
  }

  return NULL;
}

TEST_F(ScreenLayoutObserverTest, DisplayNotifications) {
  Shell::GetInstance()
      ->screen_layout_observer()
      ->set_show_notifications_for_testing(true);

  UpdateDisplay("400x400");
  display::Display::SetInternalDisplayId(display_manager()->first_display_id());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // rotation.
  UpdateDisplay("400x400/r");
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED, GetFirstDisplayName(),
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_90)),
            GetDisplayNotificationAdditionalText());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  CloseNotification();
  UpdateDisplay("400x400");
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED, GetFirstDisplayName(),
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_DISPLAY_STANDARD_ORIENTATION)),
            GetDisplayNotificationAdditionalText());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // UI-scale
  CloseNotification();
  UpdateDisplay("400x400@1.5");
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
                GetFirstDisplayName(), base::UTF8ToUTF16("600x600")),
            GetDisplayNotificationAdditionalText());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // UI-scale to 1.0
  CloseNotification();
  UpdateDisplay("400x400");
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
                GetFirstDisplayName(), base::UTF8ToUTF16("400x400")),
            GetDisplayNotificationAdditionalText());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // No-update
  CloseNotification();
  UpdateDisplay("400x400");
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Extended.
  CloseNotification();
  UpdateDisplay("400x400,200x200");
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED,
                                       GetSecondDisplayName()),
            GetDisplayNotificationText());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Mirroring.
  CloseNotification();
  display_manager()->SetSoftwareMirroring(true);
  UpdateDisplay("400x400,200x200");
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                       GetMirroringDisplayName()),
            GetDisplayNotificationText());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Back to extended.
  CloseNotification();
  display_manager()->SetSoftwareMirroring(false);
  UpdateDisplay("400x400,200x200");
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED,
                                       GetSecondDisplayName()),
            GetDisplayNotificationText());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Resize the first display.
  UpdateDisplay("400x400@1.5,200x200");
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
                GetFirstDisplayName(), base::UTF8ToUTF16("600x600")),
            GetDisplayNotificationAdditionalText());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // Rotate the second.
  UpdateDisplay("400x400@1.5,200x200/r");
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED, GetSecondDisplayName(),
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_90)),
            GetDisplayNotificationAdditionalText());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // Enters closed lid mode.
  UpdateDisplay("400x400@1.5,200x200");
  display::Display::SetInternalDisplayId(
      display_manager()->GetSecondaryDisplay().id());
  UpdateDisplay("400x400@1.5");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_DOCKED),
            GetDisplayNotificationText());
  EXPECT_EQ(ash::SubstituteChromeOSDeviceType(
                IDS_ASH_STATUS_TRAY_DISPLAY_DOCKED_DESCRIPTION),
            GetDisplayNotificationAdditionalText());
}

// Verify that notification shows up when display is switched from dock mode to
// extend mode.
TEST_F(ScreenLayoutObserverTest, DisplayConfigurationChangedTwice) {
  Shell::GetInstance()
      ->screen_layout_observer()
      ->set_show_notifications_for_testing(true);
  UpdateDisplay("400x400,200x200");
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL),
            GetDisplayNotificationText());

  // OnDisplayConfigurationChanged() may be called more than once for a single
  // update display in case of primary is swapped or recovered from dock mode.
  // Should not remove the notification in such case.
  GetScreenLayoutObserver()->OnDisplayConfigurationChanged();
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL),
            GetDisplayNotificationText());

  // Back to the single display. It SHOULD remove the notification since the
  // information is stale.
  UpdateDisplay("400x400");
  EXPECT_TRUE(GetDisplayNotificationText().empty());
}

// Verify the notification message content when one of the 2 displays that
// connected to the device is rotated.
TEST_F(ScreenLayoutObserverTest, UpdateAfterSuppressDisplayNotification) {
  UpdateDisplay("400x400,200x200");

  Shell::GetInstance()
      ->screen_layout_observer()
      ->set_show_notifications_for_testing(true);

  // Rotate the second.
  UpdateDisplay("400x400,200x200/r");
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED, GetSecondDisplayName(),
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_90)),
            GetDisplayNotificationAdditionalText());
}

// Verify that no notification is shown when overscan of a screen is changed.
TEST_F(ScreenLayoutObserverTest, OverscanDisplay) {
  UpdateDisplay("400x400, 300x300");
  Shell::GetInstance()
      ->screen_layout_observer()
      ->set_show_notifications_for_testing(true);
  display::Display::SetInternalDisplayId(display_manager()->first_display_id());

  // /o creates the default overscan.
  UpdateDisplay("400x400, 300x300/o");
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Reset the overscan.
  Shell::GetInstance()->display_manager()->SetOverscanInsets(
      display_manager()->GetSecondaryDisplay().id(), gfx::Insets());
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());
}

}  // namespace ash
