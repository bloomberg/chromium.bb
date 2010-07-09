// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/views/accessibility_event_router_views.h"
#include "chrome/browser/views/accessible_view_helper.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/button/native_button.h"
#include "views/grid_layout.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#elif defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

#if defined(TOOLKIT_VIEWS)

class AccessibilityEventRouterViewsTest
    : public testing::Test,
      public NotificationObserver {
 public:
  views::Widget* CreateWidget() {
#if defined(OS_WIN)
    return new views::WidgetWin();
#elif defined(OS_LINUX)
    return new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
#endif
  }
 protected:
  // Implement NotificationObserver::Observe and store information about a
  // ACCESSIBILITY_CONTROL_FOCUSED event.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    ASSERT_EQ(type.value, NotificationType::ACCESSIBILITY_CONTROL_FOCUSED);
    const AccessibilityControlInfo* info =
        Details<const AccessibilityControlInfo>(details).ptr();
    focus_event_count_++;
    last_control_name_ = info->name();
  }

  MessageLoopForUI message_loop_;
  int focus_event_count_;
  std::string last_control_name_;
};

// Temporarily disabled due to http://crbug.com/48717
TEST_F(AccessibilityEventRouterViewsTest, DISABLED_TestFocusNotification) {
  const char kButton1ASCII[] = "Button1";
  const char kButton2ASCII[] = "Button2";
  const char kButton3ASCII[] = "Button3";

  // Create a window and layout.
  views::Widget* window = CreateWidget();
  window->Init(NULL, gfx::Rect(0, 0, 100, 100));
  views::RootView* root_view = window->GetRootView();
  views::GridLayout* layout = new views::GridLayout(root_view);
  root_view->SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  // Add 3 buttons.
  views::NativeButton* button1 = new views::NativeButton(
      NULL, ASCIIToWide(kButton1ASCII));
  layout->StartRow(0, 0);
  layout->AddView(button1);
  views::NativeButton* button2 = new views::NativeButton(
      NULL, ASCIIToWide(kButton2ASCII));
  layout->StartRow(0, 0);
  layout->AddView(button2);
  views::NativeButton* button3 = new views::NativeButton(
      NULL, ASCIIToWide(kButton3ASCII));
  layout->StartRow(0, 0);
  layout->AddView(button3);

  // Set focus to the first button initially.
  button1->RequestFocus();

  // Start listening to ACCESSIBILITY_CONTROL_FOCUSED notifications.
  NotificationRegistrar registrar;
  registrar.Add(this,
                NotificationType::ACCESSIBILITY_CONTROL_FOCUSED,
                NotificationService::AllSources());

  // Switch on accessibility event notifications.
  TestingProfile profile;
  ExtensionAccessibilityEventRouter* accessibility_event_router =
      ExtensionAccessibilityEventRouter::GetInstance();
  accessibility_event_router->SetAccessibilityEnabled(true);

  // Create an AccessibleViewHelper for this window, which will send
  // accessibility notifications for all events that happen in child views.
  // Tell it to ignore button 3.
  AccessibleViewHelper accessible_view_helper(root_view, &profile);
  accessible_view_helper.IgnoreView(button3);

  // Advance focus to the next button and test that we got the
  // expected notification with the name of button 2.
  views::FocusManager* focus_manager = window->GetFocusManager();
  focus_event_count_ = 0;
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(1, focus_event_count_);
  EXPECT_EQ(kButton2ASCII, last_control_name_);

  // Advance to button 3. We expect no notification because we told it
  // to ignore this view.
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(1, focus_event_count_);

  // Advance to button 1 and check the notification.
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(2, focus_event_count_);
  EXPECT_EQ(kButton1ASCII, last_control_name_);
}

#endif  // defined(TOOLKIT_VIEWS)
