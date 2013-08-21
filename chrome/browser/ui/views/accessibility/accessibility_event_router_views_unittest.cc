// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/ui/views/accessibility/accessibility_event_router_views.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_helper.h"
#endif

namespace {

// The expected initial focus count.
#if defined(OS_WIN) && !defined(USE_AURA)
// On windows (non-aura) this code triggers activating the window. Activating
// the window triggers clearing the focus then resetting it. This results in an
// additional focus change.
const int kInitialFocusCount = 2;
#else
const int kInitialFocusCount = 1;
#endif

}  // namespace

class AccessibilityViewsDelegate : public views::TestViewsDelegate {
 public:
  AccessibilityViewsDelegate() {}
  virtual ~AccessibilityViewsDelegate() {}

  // Overridden from views::TestViewsDelegate:
  virtual void NotifyAccessibilityEvent(
      views::View* view, ui::AccessibilityTypes::Event event_type) OVERRIDE {
    AccessibilityEventRouterViews::GetInstance()->HandleAccessibilityEvent(
        view, event_type);
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

  DISALLOW_COPY_AND_ASSIGN(AccessibilityWindowDelegate);
};

class ViewWithNameAndRole : public views::View {
 public:
  explicit ViewWithNameAndRole(const string16& name,
                               ui::AccessibilityTypes::Role role)
      : name_(name),
        role_(role) {
  }

  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    views::View::GetAccessibleState(state);
    state->name = name_;
    state->role = role_;
  }

  void set_name(const string16& name) { name_ = name; }

 private:
  string16 name_;
  ui::AccessibilityTypes::Role role_;
  DISALLOW_COPY_AND_ASSIGN(ViewWithNameAndRole);
};

class AccessibilityEventRouterViewsTest
    : public testing::Test {
 public:
  AccessibilityEventRouterViewsTest() : focus_event_count_(0) {
  }

  virtual void SetUp() {
#if defined(OS_WIN)
    ole_initializer_.reset(new ui::ScopedOleInitializer());
#endif
    views::ViewsDelegate::views_delegate = new AccessibilityViewsDelegate();
#if defined(USE_AURA)
    aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    aura_test_helper_->SetUp();
#endif  // USE_AURA
    EnableAccessibilityAndListenToFocusNotifications();
  }

  virtual void TearDown() {
    ClearCallback();
#if defined(USE_AURA)
    aura_test_helper_->TearDown();
#endif
    delete views::ViewsDelegate::views_delegate;
    views::ViewsDelegate::views_delegate = NULL;

    // The Widget's FocusManager is deleted using DeleteSoon - this
    // forces it to be deleted now, so we don't have any memory leaks
    // when this method exits.
    base::MessageLoop::current()->RunUntilIdle();

#if defined(OS_WIN)
    ole_initializer_.reset();
#endif
  }

  views::Widget* CreateWindowWithContents(views::View* contents) {
    gfx::NativeView context = NULL;
#if defined(USE_AURA)
    context = aura_test_helper_->root_window();
#endif
    views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
        new AccessibilityWindowDelegate(contents),
        context,
        gfx::Rect(0, 0, 500, 500));

    // Create a profile and associate it with this window.
    widget->SetNativeWindowProperty(Profile::kProfileKey, &profile_);

    return widget;
  }

  void EnableAccessibilityAndListenToFocusNotifications() {
    // Switch on accessibility event notifications.
    ExtensionAccessibilityEventRouter* accessibility_event_router =
        ExtensionAccessibilityEventRouter::GetInstance();
    accessibility_event_router->SetAccessibilityEnabled(true);
    accessibility_event_router->SetControlEventCallbackForTesting(base::Bind(
        &AccessibilityEventRouterViewsTest::OnFocusEvent,
        base::Unretained(this)));
  }

  void ClearCallback() {
    ExtensionAccessibilityEventRouter* accessibility_event_router =
        ExtensionAccessibilityEventRouter::GetInstance();
    accessibility_event_router->ClearControlEventCallback();
  }

 protected:
  // Handle Focus event.
  virtual void OnFocusEvent(ui::AccessibilityTypes::Event event,
                            const AccessibilityControlInfo* info) {
    focus_event_count_++;
    last_control_name_ = info->name();
    last_control_context_ = info->context();
  }

  base::MessageLoopForUI message_loop_;
  int focus_event_count_;
  std::string last_control_name_;
  std::string last_control_context_;
  TestingProfile profile_;
#if defined(OS_WIN)
  scoped_ptr<ui::ScopedOleInitializer> ole_initializer_;
#endif
#if defined(USE_AURA)
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;
#endif
};

TEST_F(AccessibilityEventRouterViewsTest, TestFocusNotification) {
  const char kButton1ASCII[] = "Button1";
  const char kButton2ASCII[] = "Button2";
  const char kButton3ASCII[] = "Button3";
  const char kButton3NewASCII[] = "Button3New";

  // Create a contents view with 3 buttons.
  views::View* contents = new views::View();
  views::LabelButton* button1 = new views::LabelButton(
      NULL, ASCIIToUTF16(kButton1ASCII));
  button1->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  contents->AddChildView(button1);
  views::LabelButton* button2 = new views::LabelButton(
      NULL, ASCIIToUTF16(kButton2ASCII));
  button2->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  contents->AddChildView(button2);
  views::LabelButton* button3 = new views::LabelButton(
      NULL, ASCIIToUTF16(kButton3ASCII));
  button3->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  contents->AddChildView(button3);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);
  window->Show();
  window->Activate();

  // Set focus to the first button initially and run message loop to execute
  // callback.
  button1->RequestFocus();
  base::MessageLoop::current()->RunUntilIdle();

  // Change the accessible name of button3.
  button3->SetAccessibleName(ASCIIToUTF16(kButton3NewASCII));

  // Advance focus to the next button and test that we got the
  // expected notification with the name of button 2.
  views::FocusManager* focus_manager = contents->GetWidget()->GetFocusManager();
  focus_event_count_ = 0;
  focus_manager->AdvanceFocus(false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, focus_event_count_);
  EXPECT_EQ(kButton2ASCII, last_control_name_);

  // Advance to button 3. Expect the new accessible name we assigned.
  focus_manager->AdvanceFocus(false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, focus_event_count_);
  EXPECT_EQ(kButton3NewASCII, last_control_name_);

  // Advance to button 1 and check the notification.
  focus_manager->AdvanceFocus(false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(3, focus_event_count_);
  EXPECT_EQ(kButton1ASCII, last_control_name_);

  window->CloseNow();
}

TEST_F(AccessibilityEventRouterViewsTest, TestToolbarContext) {
  const char kToolbarNameASCII[] = "MyToolbar";
  const char kButtonNameASCII[] = "MyButton";

  // Create a toolbar with a button.
  views::View* contents = new ViewWithNameAndRole(
      ASCIIToUTF16(kToolbarNameASCII),
      ui::AccessibilityTypes::ROLE_TOOLBAR);
  views::LabelButton* button = new views::LabelButton(
      NULL, ASCIIToUTF16(kButtonNameASCII));
  button->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  contents->AddChildView(button);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);

  // Set focus to the button.
  focus_event_count_ = 0;
  button->RequestFocus();

  base::MessageLoop::current()->RunUntilIdle();

  // Test that we got the event with the expected name and context.
  EXPECT_EQ(kInitialFocusCount, focus_event_count_);
  EXPECT_EQ(kButtonNameASCII, last_control_name_);
  EXPECT_EQ(kToolbarNameASCII, last_control_context_);

  window->CloseNow();
}

TEST_F(AccessibilityEventRouterViewsTest, TestAlertContext) {
  const char kAlertTextASCII[] = "MyAlertText";
  const char kButtonNameASCII[] = "MyButton";

  // Create an alert with static text and a button, similar to an infobar.
  views::View* contents = new ViewWithNameAndRole(
      string16(),
      ui::AccessibilityTypes::ROLE_ALERT);
  views::Label* label = new views::Label(ASCIIToUTF16(kAlertTextASCII));
  contents->AddChildView(label);
  views::LabelButton* button = new views::LabelButton(
      NULL, ASCIIToUTF16(kButtonNameASCII));
  button->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  contents->AddChildView(button);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);

  // Set focus to the button.
  focus_event_count_ = 0;
  button->RequestFocus();

  base::MessageLoop::current()->RunUntilIdle();

  // Test that we got the event with the expected name and context.
  EXPECT_EQ(kInitialFocusCount, focus_event_count_);
  EXPECT_EQ(kButtonNameASCII, last_control_name_);
  EXPECT_EQ(kAlertTextASCII, last_control_context_);

  window->CloseNow();
}

TEST_F(AccessibilityEventRouterViewsTest, StateChangeAfterNotification) {
  const char kContentsNameASCII[] = "Contents";
  const char kOldNameASCII[] = "OldName";
  const char kNewNameASCII[] = "NewName";

  // Create a toolbar with a button.
  views::View* contents = new ViewWithNameAndRole(
      ASCIIToUTF16(kContentsNameASCII),
      ui::AccessibilityTypes::ROLE_CLIENT);
  ViewWithNameAndRole* child = new ViewWithNameAndRole(
      ASCIIToUTF16(kOldNameASCII),
      ui::AccessibilityTypes::ROLE_PUSHBUTTON);
  child->set_focusable(true);
  contents->AddChildView(child);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);

  // Set focus to the child view.
  focus_event_count_ = 0;
  child->RequestFocus();

  // Change the child's name after the focus notification.
  child->set_name(ASCIIToUTF16(kNewNameASCII));

  // We shouldn't get the notification right away.
  EXPECT_EQ(0, focus_event_count_);

  // Process anything in the event loop. Now we should get the notification,
  // and it should give us the new control name, not the old one.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kInitialFocusCount, focus_event_count_);
  EXPECT_EQ(kNewNameASCII, last_control_name_);

  window->CloseNow();
}

TEST_F(AccessibilityEventRouterViewsTest, NotificationOnDeletedObject) {
  const char kContentsNameASCII[] = "Contents";
  const char kNameASCII[] = "OldName";

  // Create a toolbar with a button.
  views::View* contents = new ViewWithNameAndRole(
      ASCIIToUTF16(kContentsNameASCII),
      ui::AccessibilityTypes::ROLE_CLIENT);
  ViewWithNameAndRole* child = new ViewWithNameAndRole(
      ASCIIToUTF16(kNameASCII),
      ui::AccessibilityTypes::ROLE_PUSHBUTTON);
  child->set_focusable(true);
  contents->AddChildView(child);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);

  // Set focus to the child view.
  focus_event_count_ = 0;
  child->RequestFocus();

  // Delete the child!
  delete child;

  // We shouldn't get the notification right away.
  EXPECT_EQ(0, focus_event_count_);

  // Process anything in the event loop. We shouldn't get a notification
  // because the view is no longer valid, and this shouldn't crash.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, focus_event_count_);

  window->CloseNow();
}
