// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/ui/views/accessibility_event_router_views.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(TOOLKIT_VIEWS)

class AccessibilityViewsDelegate : public views::ViewsDelegate {
 public:
  AccessibilityViewsDelegate() {}
  virtual ~AccessibilityViewsDelegate() {}

  // Overridden from views::ViewsDelegate:
  virtual ui::Clipboard* GetClipboard() const OVERRIDE { return NULL; }
  virtual void SaveWindowPlacement(const views::Widget* window,
                                   const std::string& window_name,
                                   const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) OVERRIDE {
  }
  virtual bool GetSavedWindowPlacement(
      const std::string& window_name,
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE {
    return false;
  }
  virtual void NotifyAccessibilityEvent(
      views::View* view, ui::AccessibilityTypes::Event event_type) OVERRIDE {
    AccessibilityEventRouterViews::GetInstance()->HandleAccessibilityEvent(
        view, event_type);
  }
  virtual void NotifyMenuItemFocused(const string16& menu_name,
                                     const string16& menu_item_name,
                                     int item_index,
                                     int item_count,
                                     bool has_submenu) OVERRIDE {}
#if defined(OS_WIN)
  virtual HICON GetDefaultWindowIcon() const OVERRIDE {
    return NULL;
  }
#endif
  virtual void AddRef() OVERRIDE {}
  virtual void ReleaseRef() OVERRIDE {}

  virtual int GetDispositionForEvent(int event_flags) OVERRIDE {
    return 0;
  }

  DISALLOW_COPY_AND_ASSIGN(AccessibilityViewsDelegate);
};

class AccessibilityWindowDelegate : public views::WidgetDelegate {
 public:
  explicit AccessibilityWindowDelegate(views::View* contents)
      : contents_(contents) { }

  // Overridden from views::WidgetDelegate:
  virtual void DeleteDelegate() OVERRIDE { delete this; }
  virtual views::View* GetContentsView() OVERRIDE { return contents_; }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return contents_->GetWidget();
  }
  virtual views::Widget* GetWidget() OVERRIDE { return contents_->GetWidget(); }

 private:
  views::View* contents_;
};

class AccessibilityEventRouterViewsTest
    : public testing::Test,
      public content::NotificationObserver {
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

  views::Widget* CreateWindowWithContents(views::View* contents) {
    window_delegate_ = new AccessibilityWindowDelegate(contents);
    return views::Widget::CreateWindowWithBounds(window_delegate_,
                                                 gfx::Rect(0, 0, 500, 500));
  }

 protected:
  // Implement NotificationObserver::Observe and store information about a
  // ACCESSIBILITY_CONTROL_FOCUSED event.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    ASSERT_EQ(type, chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED);
    const AccessibilityControlInfo* info =
        content::Details<const AccessibilityControlInfo>(details).ptr();
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
  views::NativeTextButton* button1 = new views::NativeTextButton(
      NULL, ASCIIToUTF16(kButton1ASCII));
  contents->AddChildView(button1);
  views::NativeTextButton* button2 = new views::NativeTextButton(
      NULL, ASCIIToUTF16(kButton2ASCII));
  contents->AddChildView(button2);
  views::NativeTextButton* button3 = new views::NativeTextButton(
      NULL, ASCIIToUTF16(kButton3ASCII));
  contents->AddChildView(button3);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);

  // Set focus to the first button initially.
  button1->RequestFocus();

  // Start listening to ACCESSIBILITY_CONTROL_FOCUSED notifications.
  content::NotificationRegistrar registrar;
  registrar.Add(this,
                chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED,
                content::NotificationService::AllSources());

  // Switch on accessibility event notifications.
  ExtensionAccessibilityEventRouter* accessibility_event_router =
      ExtensionAccessibilityEventRouter::GetInstance();
  accessibility_event_router->SetAccessibilityEnabled(true);

  // Create a profile and associate it with this window.
  TestingProfile profile;
  window->SetNativeWindowProperty(Profile::kProfileKey, &profile);

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
