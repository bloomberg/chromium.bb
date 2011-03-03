// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/ui/views/accessibility_event_router_views.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/button/native_button.h"
#include "views/layout/grid_layout.h"
#include "views/views_delegate.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"
#include "views/window/window_delegate.h"

#if defined(TOOLKIT_VIEWS)

class AccessibilityViewsDelegate : public views::ViewsDelegate {
 public:
  AccessibilityViewsDelegate() {}
  virtual ~AccessibilityViewsDelegate() {}

  // Overridden from views::ViewsDelegate:
  virtual ui::Clipboard* GetClipboard() const { return NULL; }
  virtual void SaveWindowPlacement(views::Window* window,
                                   const std::wstring& window_name,
                                   const gfx::Rect& bounds,
                                   bool maximized) {
  }
  virtual bool GetSavedWindowBounds(views::Window* window,
                                    const std::wstring& window_name,
                                    gfx::Rect* bounds) const {
    return false;
  }
  virtual bool GetSavedMaximizedState(views::Window* window,
                                      const std::wstring& window_name,
                                      bool* maximized) const {
    return false;
  }
  virtual void NotifyAccessibilityEvent(
      views::View* view, ui::AccessibilityTypes::Event event_type) {
    AccessibilityEventRouterViews::GetInstance()->HandleAccessibilityEvent(
        view, event_type);
  }
#if defined(OS_WIN)
  virtual HICON GetDefaultWindowIcon() const {
    return NULL;
  }
#endif
  virtual void AddRef() {}
  virtual void ReleaseRef() {}

  DISALLOW_COPY_AND_ASSIGN(AccessibilityViewsDelegate);
};

class AccessibilityWindowDelegate : public views::WindowDelegate {
 public:
  explicit AccessibilityWindowDelegate(views::View* contents)
      : contents_(contents) { }

  virtual void DeleteDelegate() { delete this; }

  virtual views::View* GetContentsView() { return contents_; }

 private:
  views::View* contents_;
};

class AccessibilityEventRouterViewsTest
    : public testing::Test,
      public NotificationObserver {
 public:
  virtual void SetUp() {
    views::ViewsDelegate::views_delegate = new AccessibilityViewsDelegate();
    window_delegate_ = NULL;
  }

  virtual void TearDown() {
    delete views::ViewsDelegate::views_delegate;
    views::ViewsDelegate::views_delegate = NULL;
    if (window_delegate_)
      delete window_delegate_;
  }

  views::Window* CreateWindowWithContents(views::View* contents) {
    window_delegate_ = new AccessibilityWindowDelegate(contents);
    return views::Window::CreateChromeWindow(
        NULL, gfx::Rect(0, 0, 500, 500), window_delegate_);
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
  AccessibilityWindowDelegate* window_delegate_;
};

TEST_F(AccessibilityEventRouterViewsTest, TestFocusNotification) {
  const char kButton1ASCII[] = "Button1";
  const char kButton2ASCII[] = "Button2";
  const char kButton3ASCII[] = "Button3";
  const char kButton3NewASCII[] = "Button3New";

  // Create a contents view with 3 buttons.
  views::View* contents = new views::View();
  views::NativeButton* button1 = new views::NativeButton(
      NULL, ASCIIToWide(kButton1ASCII));
  contents->AddChildView(button1);
  views::NativeButton* button2 = new views::NativeButton(
      NULL, ASCIIToWide(kButton2ASCII));
  contents->AddChildView(button2);
  views::NativeButton* button3 = new views::NativeButton(
      NULL, ASCIIToWide(kButton3ASCII));
  contents->AddChildView(button3);

  // Put the view in a window.
  views::Window* window = CreateWindowWithContents(contents);

  // Set focus to the first button initially.
  button1->RequestFocus();

  // Start listening to ACCESSIBILITY_CONTROL_FOCUSED notifications.
  NotificationRegistrar registrar;
  registrar.Add(this,
                NotificationType::ACCESSIBILITY_CONTROL_FOCUSED,
                NotificationService::AllSources());

  // Switch on accessibility event notifications.
  ExtensionAccessibilityEventRouter* accessibility_event_router =
      ExtensionAccessibilityEventRouter::GetInstance();
  accessibility_event_router->SetAccessibilityEnabled(true);

  // Create a profile and associate it with this window.
  TestingProfile profile;
  window->SetNativeWindowProperty(
      Profile::kProfileKey, &profile);

  // Change the accessible name of button3.
  button3->SetAccessibleName(ASCIIToUTF16(kButton3NewASCII));

  // Advance focus to the next button and test that we got the
  // expected notification with the name of button 2.
  views::FocusManager* focus_manager = contents->GetWidget()->GetFocusManager();
  focus_event_count_ = 0;
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(1, focus_event_count_);
  EXPECT_EQ(kButton2ASCII, last_control_name_);

  // Advance to button 3. Expect the new accessible name we assigned.
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(2, focus_event_count_);
  EXPECT_EQ(kButton3NewASCII, last_control_name_);

  // Advance to button 1 and check the notification.
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(3, focus_event_count_);
  EXPECT_EQ(kButton1ASCII, last_control_name_);
}

#endif  // defined(TOOLKIT_VIEWS)
