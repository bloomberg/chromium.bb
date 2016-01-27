// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include <utility>
#include <vector>

#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/web_notification/ash_popup_alignment_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/wm/window_state.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

WebNotificationTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->
      web_notification_tray();
}

WebNotificationTray* GetSecondaryTray() {
  StatusAreaWidget* status_area_widget =
      StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget();
  if (status_area_widget)
    return status_area_widget->web_notification_tray();
  return NULL;
}

message_center::MessageCenter* GetMessageCenter() {
  return GetTray()->message_center();
}

SystemTray* GetSystemTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->system_tray();
}

// Trivial item implementation for testing PopupAndSystemTray test case.
class TestItem : public SystemTrayItem {
 public:
  TestItem() : SystemTrayItem(GetSystemTray()) {}

  views::View* CreateDefaultView(user::LoginStatus status) override {
    views::View* default_view = new views::View;
    default_view->SetLayoutManager(new views::FillLayout);
    default_view->AddChildView(new views::Label(base::UTF8ToUTF16("Default")));
    return default_view;
  }

  views::View* CreateNotificationView(user::LoginStatus status) override {
    return new views::View;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestItem);
};

}  // namespace

class WebNotificationTrayTest : public test::AshTestBase {
 public:
  WebNotificationTrayTest() {}
  ~WebNotificationTrayTest() override {}

  void TearDown() override {
    GetMessageCenter()->RemoveAllNotifications(false);
    test::AshTestBase::TearDown();
  }

 protected:
  void AddNotification(const std::string& id) {
    scoped_ptr<message_center::Notification> notification;
    notification.reset(new message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, id,
        base::ASCIIToUTF16("Test Web Notification"),
        base::ASCIIToUTF16("Notification message body."), gfx::Image(),
        base::ASCIIToUTF16("www.test.org"), GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        NULL /* delegate */));
    GetMessageCenter()->AddNotification(std::move(notification));
  }

  void UpdateNotification(const std::string& old_id,
                          const std::string& new_id) {
    scoped_ptr<message_center::Notification> notification;
    notification.reset(new message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, new_id,
        base::ASCIIToUTF16("Updated Web Notification"),
        base::ASCIIToUTF16("Updated message body."), gfx::Image(),
        base::ASCIIToUTF16("www.test.org"), GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        NULL /* delegate */));
    GetMessageCenter()->UpdateNotification(old_id, std::move(notification));
  }

  void RemoveNotification(const std::string& id) {
    GetMessageCenter()->RemoveNotification(id, false);
  }

  views::Widget* GetWidget() {
    return GetTray()->GetWidget();
  }

  int GetPopupWorkAreaBottom() {
    return GetPopupWorkAreaBottomForTray(GetTray());
  }

  int GetPopupWorkAreaBottomForTray(WebNotificationTray* tray) {
    return tray->popup_alignment_delegate_->GetWorkAreaBottom();
  }

  bool IsPopupVisible() {
    return GetTray()->IsPopupVisible();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationTrayTest);
};

TEST_F(WebNotificationTrayTest, WebNotifications) {
  // TODO(mukai): move this test case to ui/message_center.
  ASSERT_TRUE(GetWidget());

  // Add a notification.
  AddNotification("test_id1");
  EXPECT_EQ(1u, GetMessageCenter()->NotificationCount());
  EXPECT_TRUE(GetMessageCenter()->FindVisibleNotificationById("test_id1"));
  AddNotification("test_id2");
  AddNotification("test_id2");
  EXPECT_EQ(2u, GetMessageCenter()->NotificationCount());
  EXPECT_TRUE(GetMessageCenter()->FindVisibleNotificationById("test_id2"));

  // Ensure that updating a notification does not affect the count.
  UpdateNotification("test_id2", "test_id3");
  UpdateNotification("test_id3", "test_id3");
  EXPECT_EQ(2u, GetMessageCenter()->NotificationCount());
  EXPECT_FALSE(GetMessageCenter()->FindVisibleNotificationById("test_id2"));

  // Ensure that Removing the first notification removes it from the tray.
  RemoveNotification("test_id1");
  EXPECT_FALSE(GetMessageCenter()->FindVisibleNotificationById("test_id1"));
  EXPECT_EQ(1u, GetMessageCenter()->NotificationCount());

  // Remove the remianing notification.
  RemoveNotification("test_id3");
  EXPECT_EQ(0u, GetMessageCenter()->NotificationCount());
  EXPECT_FALSE(GetMessageCenter()->FindVisibleNotificationById("test_id3"));
}

TEST_F(WebNotificationTrayTest, WebNotificationPopupBubble) {
  // TODO(mukai): move this test case to ui/message_center.
  ASSERT_TRUE(GetWidget());

  // Adding a notification should show the popup bubble.
  AddNotification("test_id1");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  // Updating a notification should not hide the popup bubble.
  AddNotification("test_id2");
  UpdateNotification("test_id2", "test_id3");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  // Removing the first notification should not hide the popup bubble.
  RemoveNotification("test_id1");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  // Removing the visible notification should hide the popup bubble.
  RemoveNotification("test_id3");
  EXPECT_FALSE(GetTray()->IsPopupVisible());

  // Now test that we can show multiple popups and then show the message center.
  AddNotification("test_id4");
  AddNotification("test_id5");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  GetTray()->message_center_tray_->ShowMessageCenterBubble();
  GetTray()->message_center_tray_->HideMessageCenterBubble();

  EXPECT_FALSE(GetTray()->IsPopupVisible());
}

using message_center::NotificationList;


// Flakily fails. http://crbug.com/229791
TEST_F(WebNotificationTrayTest, DISABLED_ManyMessageCenterNotifications) {
  // Add the max visible notifications +1, ensure the correct visible number.
  size_t notifications_to_add =
      message_center::kMaxVisibleMessageCenterNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("test_id%d", static_cast<int>(i));
    AddNotification(id);
  }
  bool shown = GetTray()->message_center_tray_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(GetTray()->message_center_bubble() != NULL);
  EXPECT_EQ(notifications_to_add,
            GetMessageCenter()->NotificationCount());
  EXPECT_EQ(message_center::kMaxVisibleMessageCenterNotifications,
            GetTray()->GetMessageCenterBubbleForTest()->
                NumMessageViewsForTest());
}

// Flakily times out. http://crbug.com/229792
TEST_F(WebNotificationTrayTest, DISABLED_ManyPopupNotifications) {
  // Add the max visible popup notifications +1, ensure the correct num visible.
  size_t notifications_to_add =
      message_center::kMaxVisiblePopupNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("test_id%d", static_cast<int>(i));
    AddNotification(id);
  }
  GetTray()->ShowPopups();
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_EQ(notifications_to_add,
            GetMessageCenter()->NotificationCount());
  NotificationList::PopupNotifications popups =
      GetMessageCenter()->GetPopupNotifications();
  EXPECT_EQ(message_center::kMaxVisiblePopupNotifications, popups.size());
}

#if defined(OS_CHROMEOS)
// Display notification is ChromeOS only.
#define MAYBE_PopupShownOnBothDisplays PopupShownOnBothDisplays
#define MAYBE_PopupAndSystemTrayMultiDisplay PopupAndSystemTrayMultiDisplay
#else
#define MAYBE_PopupShownOnBothDisplays DISABLED_PopupShownOnBothDisplays
#define MAYBE_PopupAndSystemTrayMultiDisplay \
  DISABLED_PopupAndSystemTrayMultiDisplay
#endif

// Verifies if the notification appears on both displays when extended mode.
TEST_F(WebNotificationTrayTest, MAYBE_PopupShownOnBothDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  // Enables to appear the notification for display changes.
  test::TestSystemTrayDelegate* tray_delegate =
      static_cast<test::TestSystemTrayDelegate*>(
          Shell::GetInstance()->system_tray_delegate());
  tray_delegate->set_should_show_display_notification(true);

  UpdateDisplay("400x400,200x200");
  // UpdateDisplay() creates the display notifications, so popup is visible.
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  WebNotificationTray* secondary_tray = GetSecondaryTray();
  ASSERT_TRUE(secondary_tray);
  EXPECT_TRUE(secondary_tray->IsPopupVisible());

  // Transition to mirroring and then back to extended display, which recreates
  // root window controller and shelf with having notifications. This code
  // verifies it doesn't cause crash and popups are still visible. See
  // http://crbug.com/263664
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  display_manager->SetMultiDisplayMode(DisplayManager::MIRRORING);
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_FALSE(GetSecondaryTray());

  display_manager->SetMultiDisplayMode(DisplayManager::EXTENDED);
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  secondary_tray = GetSecondaryTray();
  ASSERT_TRUE(secondary_tray);
  EXPECT_TRUE(secondary_tray->IsPopupVisible());
}

#if defined(OS_CHROMEOS)
// PopupAndSystemTray may fail in platforms other than ChromeOS because the
// RootWindow's bound can be bigger than gfx::Display's work area so that
// openingsystem tray doesn't affect at all the work area of popups.
#define MAYBE_PopupAndSystemTray PopupAndSystemTray
#define MAYBE_PopupAndAutoHideShelf PopupAndAutoHideShelf
#define MAYBE_PopupAndFullscreen PopupAndFullscreen
#else
#define MAYBE_PopupAndSystemTray DISABLED_PopupAndSystemTray
#define MAYBE_PopupAndAutoHideShelf DISABLED_PopupAndAutoHideShelf
#define MAYBE_PopupAndFullscreen DISABLED_PopupAndFullscreen
#endif

TEST_F(WebNotificationTrayTest, MAYBE_PopupAndSystemTray) {
  TestItem* test_item = new TestItem;
  GetSystemTray()->AddTrayItem(test_item);

  AddNotification("test_id");
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom = GetPopupWorkAreaBottom();

  // System tray is created, the popup's work area should be narrowed but still
  // visible.
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom_with_tray = GetPopupWorkAreaBottom();
  EXPECT_GT(bottom, bottom_with_tray);

  // System tray notification is also created, the popup's work area is narrowed
  // even more, but still visible.
  GetSystemTray()->ShowNotificationView(test_item);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom_with_tray_notification = GetPopupWorkAreaBottom();
  EXPECT_GT(bottom, bottom_with_tray_notification);
  EXPECT_GT(bottom_with_tray, bottom_with_tray_notification);

  // Close system tray, only system tray notifications.
  GetSystemTray()->ClickedOutsideBubble();
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom_with_notification = GetPopupWorkAreaBottom();
  EXPECT_GT(bottom, bottom_with_notification);
  EXPECT_LT(bottom_with_tray_notification, bottom_with_notification);

  // Close the system tray notifications.
  GetSystemTray()->HideNotificationView(test_item);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_EQ(bottom, GetPopupWorkAreaBottom());
}

TEST_F(WebNotificationTrayTest, MAYBE_PopupAndAutoHideShelf) {
  AddNotification("test_id");
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom = GetPopupWorkAreaBottom();

  // Shelf's auto-hide state won't be HIDDEN unless window exists.
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  ShelfLayoutManager* shelf =
      Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  int bottom_auto_hidden = GetPopupWorkAreaBottom();
  EXPECT_LT(bottom, bottom_auto_hidden);

  // Close the window, which shows the shelf.
  window.reset();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  int bottom_auto_shown = GetPopupWorkAreaBottom();
  EXPECT_EQ(bottom, bottom_auto_shown);

  // Create the system tray during auto-hide.
  window.reset(CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  TestItem* test_item = new TestItem;
  GetSystemTray()->AddTrayItem(test_item);
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);

  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom_with_tray = GetPopupWorkAreaBottom();
  EXPECT_GT(bottom_auto_shown, bottom_with_tray);

  // Create tray notification.
  GetSystemTray()->ShowNotificationView(test_item);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  int bottom_with_tray_notification = GetPopupWorkAreaBottom();
  EXPECT_GT(bottom_with_tray, bottom_with_tray_notification);

  // Close the system tray.
  GetSystemTray()->ClickedOutsideBubble();
  shelf->UpdateAutoHideState();
  RunAllPendingInMessageLoop();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  int bottom_hidden_with_tray_notification = GetPopupWorkAreaBottom();
  EXPECT_LT(bottom_with_tray_notification,
            bottom_hidden_with_tray_notification);
  EXPECT_GT(bottom_auto_hidden, bottom_hidden_with_tray_notification);

  // Close the window again, which shows the shelf.
  window.reset();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  int bottom_shown_with_tray_notification = GetPopupWorkAreaBottom();
  EXPECT_GT(bottom_hidden_with_tray_notification,
            bottom_shown_with_tray_notification);
  EXPECT_GT(bottom_auto_shown, bottom_shown_with_tray_notification);
}

TEST_F(WebNotificationTrayTest, MAYBE_PopupAndFullscreen) {
  AddNotification("test_id");
  EXPECT_TRUE(IsPopupVisible());
  int bottom = GetPopupWorkAreaBottom();

  // Checks the work area for normal auto-hidden state.
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  ShelfLayoutManager* shelf =
      Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  int bottom_auto_hidden = GetPopupWorkAreaBottom();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // Put |window| into fullscreen without forcing the shelf to hide. Currently,
  // this is used by immersive fullscreen and forces the shelf to be auto
  // hidden.
  wm::GetWindowState(window.get())->set_hide_shelf_when_fullscreen(false);
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  RunAllPendingInMessageLoop();

  // The work area for auto-hidden status of fullscreen is a bit larger
  // since it doesn't even have the 3-pixel width.
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  int bottom_fullscreen_hidden = GetPopupWorkAreaBottom();
  EXPECT_EQ(bottom_auto_hidden, bottom_fullscreen_hidden);

  // Move the mouse cursor at the bottom, which shows the shelf.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  gfx::Point bottom_right =
      gfx::Screen::GetScreen()->GetPrimaryDisplay().bounds().bottom_right();
  bottom_right.Offset(-1, -1);
  generator.MoveMouseTo(bottom_right);
  shelf->UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_EQ(bottom, GetPopupWorkAreaBottom());

  generator.MoveMouseTo(
      gfx::Screen::GetScreen()->GetPrimaryDisplay().bounds().CenterPoint());
  shelf->UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(bottom_auto_hidden, GetPopupWorkAreaBottom());
}

TEST_F(WebNotificationTrayTest, MAYBE_PopupAndSystemTrayMultiDisplay) {
  UpdateDisplay("800x600,600x400");

  AddNotification("test_id");
  int bottom = GetPopupWorkAreaBottom();
  int bottom_second = GetPopupWorkAreaBottomForTray(GetSecondaryTray());

  // System tray is created on the primary display. The popups in the secondary
  // tray aren't affected.
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
  EXPECT_GT(bottom, GetPopupWorkAreaBottom());
  EXPECT_EQ(bottom_second, GetPopupWorkAreaBottomForTray(GetSecondaryTray()));
}

// TODO(jonross): Replace manually creating TouchEvent with
// EventGenerator.PressTouch/ReleaseTouch. Currently they set a width on the
// touch event causing the gesture recognizer to target a different view.
#if defined(OS_CHROMEOS)
// Tests that there is visual feedback for touch presses.
TEST_F(WebNotificationTrayTest, TouchFeedback) {
  AddNotification("test_id");
  RunAllPendingInMessageLoop();
  WebNotificationTray* tray = GetTray();
  EXPECT_TRUE(tray->visible());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  const int touch_id = 0;
  gfx::Point center_point = tray->GetBoundsInScreen().CenterPoint();

  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, center_point, touch_id,
                       generator.Now());
  generator.Dispatch(&press);
  EXPECT_TRUE(tray->draw_background_as_active());

  ui::TouchEvent release(ui::ET_TOUCH_RELEASED, center_point, touch_id,
      press.time_stamp() + base::TimeDelta::FromMilliseconds(50));
  generator.Dispatch(&release);
  EXPECT_TRUE(tray->draw_background_as_active());
  EXPECT_TRUE(tray->IsMessageCenterBubbleVisible());

  generator.GestureTapAt(center_point);
  EXPECT_FALSE(tray->draw_background_as_active());
  EXPECT_FALSE(tray->IsMessageCenterBubbleVisible());
}

// Tests that while touch presses trigger visual feedback, that subsequent non
// tap gestures cancel the feedback without triggering the message center.
TEST_F(WebNotificationTrayTest, TouchFeedbackCancellation) {
  AddNotification("test_id");
  RunAllPendingInMessageLoop();
  WebNotificationTray* tray = GetTray();
  EXPECT_TRUE(tray->visible());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  const int touch_id = 0;
  gfx::Rect bounds = tray->GetBoundsInScreen();
  gfx::Point center_point = bounds.CenterPoint();

  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, center_point, touch_id,
                       generator.Now());
  generator.Dispatch(&press);
  EXPECT_TRUE(tray->draw_background_as_active());

  gfx::Point out_of_bounds(bounds.x() - 1, center_point.y());
  ui::TouchEvent move(ui::ET_TOUCH_MOVED, out_of_bounds, touch_id,
                      press.time_stamp()+base::TimeDelta::FromMilliseconds(50));
  generator.Dispatch(&move);
  EXPECT_FALSE(tray->draw_background_as_active());

  ui::TouchEvent release(ui::ET_TOUCH_RELEASED, out_of_bounds, touch_id,
      move.time_stamp()+base::TimeDelta::FromMilliseconds(50));
  generator.Dispatch(&release);
  EXPECT_FALSE(tray->draw_background_as_active());
  EXPECT_FALSE(tray->IsMessageCenterBubbleVisible());
}

#endif  // OS_CHROMEOS

}  // namespace ash
