// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/accessibility/accessibility_extension_api_constants.h"
#include "chrome/browser/ui/views/accessibility/accessibility_event_router_views.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
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
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/wm/core/default_activation_client.h"
#endif

using base::ASCIIToUTF16;

class AccessibilityViewsDelegate : public views::TestViewsDelegate {
 public:
  AccessibilityViewsDelegate() {}
  virtual ~AccessibilityViewsDelegate() {}

  // Overridden from views::TestViewsDelegate:
  virtual void NotifyAccessibilityEvent(
      views::View* view, ui::AXEvent event_type) OVERRIDE {
    AccessibilityEventRouterViews::GetInstance()->HandleAccessibilityEvent(
        view, event_type);
  }

 private:
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
  explicit ViewWithNameAndRole(const base::string16& name,
                               ui::AXRole role)
      : name_(name),
        role_(role) {
  }

  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE {
    views::View::GetAccessibleState(state);
    state->name = name_;
    state->role = role_;
  }

  void set_name(const base::string16& name) { name_ = name; }

 private:
  base::string16 name_;
  ui::AXRole role_;
  DISALLOW_COPY_AND_ASSIGN(ViewWithNameAndRole);
};

class AccessibilityEventRouterViewsTest
    : public testing::Test {
 public:
  AccessibilityEventRouterViewsTest() : control_event_count_(0) {
  }

  virtual void SetUp() {
#if defined(OS_WIN)
    ole_initializer_.reset(new ui::ScopedOleInitializer());
#endif
    views::ViewsDelegate::views_delegate = new AccessibilityViewsDelegate();
#if defined(USE_AURA)
    // The ContextFactory must exist before any Compositors are created.
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(enable_pixel_output);

    aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    aura_test_helper_->SetUp(context_factory);
    new wm::DefaultActivationClient(aura_test_helper_->root_window());
#endif  // USE_AURA
    EnableAccessibilityAndListenToFocusNotifications();
  }

  virtual void TearDown() {
    ClearCallback();
#if defined(USE_AURA)
    aura_test_helper_->TearDown();
    ui::TerminateContextFactoryForTests();
#endif
    delete views::ViewsDelegate::views_delegate;

    // The Widget's FocusManager is deleted using DeleteSoon - this
    // forces it to be deleted now, so we don't have any memory leaks
    // when this method exits.
    base::MessageLoop::current()->RunUntilIdle();

#if defined(OS_WIN)
    ole_initializer_.reset();
#endif
  }

  views::Widget* CreateWindowWithContents(views::View* contents) {
    gfx::NativeWindow context = NULL;
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
        &AccessibilityEventRouterViewsTest::OnControlEvent,
        base::Unretained(this)));
  }

  void ClearCallback() {
    ExtensionAccessibilityEventRouter* accessibility_event_router =
        ExtensionAccessibilityEventRouter::GetInstance();
    accessibility_event_router->ClearControlEventCallback();
  }

 protected:
  // Handle Focus event.
  virtual void OnControlEvent(ui::AXEvent event,
                            const AccessibilityControlInfo* info) {
    control_event_count_++;
    last_control_type_ = info->type();
    last_control_name_ = info->name();
    last_control_context_ = info->context();
  }

  base::MessageLoopForUI message_loop_;
  int control_event_count_;
  std::string last_control_type_;
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
  button1->SetStyle(views::Button::STYLE_BUTTON);
  contents->AddChildView(button1);
  views::LabelButton* button2 = new views::LabelButton(
      NULL, ASCIIToUTF16(kButton2ASCII));
  button2->SetStyle(views::Button::STYLE_BUTTON);
  contents->AddChildView(button2);
  views::LabelButton* button3 = new views::LabelButton(
      NULL, ASCIIToUTF16(kButton3ASCII));
  button3->SetStyle(views::Button::STYLE_BUTTON);
  contents->AddChildView(button3);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);
  window->Show();

  // Set focus to the first button initially and run message loop to execute
  // callback.
  button1->RequestFocus();
  base::MessageLoop::current()->RunUntilIdle();

  // Change the accessible name of button3.
  button3->SetAccessibleName(ASCIIToUTF16(kButton3NewASCII));

  // Advance focus to the next button and test that we got the
  // expected notification with the name of button 2.
  views::FocusManager* focus_manager = contents->GetWidget()->GetFocusManager();
  control_event_count_ = 0;
  focus_manager->AdvanceFocus(false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, control_event_count_);
  EXPECT_EQ(kButton2ASCII, last_control_name_);

  // Advance to button 3. Expect the new accessible name we assigned.
  focus_manager->AdvanceFocus(false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, control_event_count_);
  EXPECT_EQ(kButton3NewASCII, last_control_name_);

  // Advance to button 1 and check the notification.
  focus_manager->AdvanceFocus(false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(3, control_event_count_);
  EXPECT_EQ(kButton1ASCII, last_control_name_);

  window->CloseNow();
}

TEST_F(AccessibilityEventRouterViewsTest, TestToolbarContext) {
  const char kToolbarNameASCII[] = "MyToolbar";
  const char kButtonNameASCII[] = "MyButton";

  // Create a toolbar with a button.
  views::View* contents = new ViewWithNameAndRole(
      ASCIIToUTF16(kToolbarNameASCII),
      ui::AX_ROLE_TOOLBAR);
  views::LabelButton* button = new views::LabelButton(
      NULL, ASCIIToUTF16(kButtonNameASCII));
  button->SetStyle(views::Button::STYLE_BUTTON);
  contents->AddChildView(button);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);

  // Set focus to the button.
  control_event_count_ = 0;
  button->RequestFocus();

  base::MessageLoop::current()->RunUntilIdle();

  // Test that we got the event with the expected name and context.
  EXPECT_EQ(1, control_event_count_);
  EXPECT_EQ(kButtonNameASCII, last_control_name_);
  EXPECT_EQ(kToolbarNameASCII, last_control_context_);

  window->CloseNow();
}

TEST_F(AccessibilityEventRouterViewsTest, TestAlertContext) {
  const char kAlertTextASCII[] = "MyAlertText";
  const char kButtonNameASCII[] = "MyButton";

  // Create an alert with static text and a button, similar to an infobar.
  views::View* contents = new ViewWithNameAndRole(
      base::string16(),
      ui::AX_ROLE_ALERT);
  views::Label* label = new views::Label(ASCIIToUTF16(kAlertTextASCII));
  contents->AddChildView(label);
  views::LabelButton* button = new views::LabelButton(
      NULL, ASCIIToUTF16(kButtonNameASCII));
  button->SetStyle(views::Button::STYLE_BUTTON);
  contents->AddChildView(button);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);

  // Set focus to the button.
  control_event_count_ = 0;
  button->RequestFocus();

  base::MessageLoop::current()->RunUntilIdle();

  // Test that we got the event with the expected name and context.
  EXPECT_EQ(1, control_event_count_);
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
      ui::AX_ROLE_CLIENT);
  ViewWithNameAndRole* child = new ViewWithNameAndRole(
      ASCIIToUTF16(kOldNameASCII),
      ui::AX_ROLE_BUTTON);
  child->SetFocusable(true);
  contents->AddChildView(child);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);

  // Set focus to the child view.
  control_event_count_ = 0;
  child->RequestFocus();

  // Change the child's name after the focus notification.
  child->set_name(ASCIIToUTF16(kNewNameASCII));

  // We shouldn't get the notification right away.
  EXPECT_EQ(0, control_event_count_);

  // Process anything in the event loop. Now we should get the notification,
  // and it should give us the new control name, not the old one.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, control_event_count_);
  EXPECT_EQ(kNewNameASCII, last_control_name_);

  window->CloseNow();
}

TEST_F(AccessibilityEventRouterViewsTest, NotificationOnDeletedObject) {
  const char kContentsNameASCII[] = "Contents";
  const char kNameASCII[] = "OldName";

  // Create a toolbar with a button.
  views::View* contents = new ViewWithNameAndRole(
      ASCIIToUTF16(kContentsNameASCII),
      ui::AX_ROLE_CLIENT);
  ViewWithNameAndRole* child = new ViewWithNameAndRole(
      ASCIIToUTF16(kNameASCII),
      ui::AX_ROLE_BUTTON);
  child->SetFocusable(true);
  contents->AddChildView(child);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);

  // Set focus to the child view.
  control_event_count_ = 0;
  child->RequestFocus();

  // Delete the child!
  delete child;

  // We shouldn't get the notification right away.
  EXPECT_EQ(0, control_event_count_);

  // Process anything in the event loop. We shouldn't get a notification
  // because the view is no longer valid, and this shouldn't crash.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, control_event_count_);

  window->CloseNow();
}

TEST_F(AccessibilityEventRouterViewsTest, AlertsFromWindowAndControl) {
  const char kButtonASCII[] = "Button";
  const char* kTypeAlert = extension_accessibility_api_constants::kTypeAlert;
  const char* kTypeWindow = extension_accessibility_api_constants::kTypeWindow;

  // Create a contents view with a button.
  views::View* contents = new views::View();
  views::LabelButton* button = new views::LabelButton(
      NULL, ASCIIToUTF16(kButtonASCII));
  button->SetStyle(views::Button::STYLE_BUTTON);
  contents->AddChildView(button);

  // Put the view in a window.
  views::Widget* window = CreateWindowWithContents(contents);
  window->Show();

  // Send an alert event from the button and let the event loop run.
  control_event_count_ = 0;
  button->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(kTypeAlert, last_control_type_);
  EXPECT_EQ(1, control_event_count_);
  EXPECT_EQ(kButtonASCII, last_control_name_);

  // Send an alert event from the window and let the event loop run.
  control_event_count_ = 0;
  window->GetRootView()->NotifyAccessibilityEvent(
      ui::AX_EVENT_ALERT, true);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(1, control_event_count_);
  EXPECT_EQ(kTypeWindow, last_control_type_);

  window->CloseNow();
}

namespace {

class SimpleMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  enum {
    IDC_MENU_ITEM_1,
    IDC_MENU_ITEM_2,
    IDC_MENU_INVISIBLE,
    IDC_MENU_ITEM_3,
  };

  SimpleMenuDelegate() {}
  virtual ~SimpleMenuDelegate() {}

  views::MenuItemView* BuildMenu() {
    menu_model_.reset(new ui::SimpleMenuModel(this));
    menu_model_->AddItem(IDC_MENU_ITEM_1, ASCIIToUTF16("Item 1"));
    menu_model_->AddItem(IDC_MENU_ITEM_2, ASCIIToUTF16("Item 2"));
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(IDC_MENU_INVISIBLE, ASCIIToUTF16("Invisible"));
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(IDC_MENU_ITEM_3, ASCIIToUTF16("Item 3"));

    menu_adapter_.reset(new views::MenuModelAdapter(menu_model_.get()));
    views::MenuItemView* menu_view = menu_adapter_->CreateMenu();
    menu_runner_.reset(new views::MenuRunner(menu_view, 0));
    return menu_view;
  }

  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return true;
  }

  virtual bool IsCommandIdVisible(int command_id) const OVERRIDE {
    return command_id != IDC_MENU_INVISIBLE;
  }

  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }

  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {
  }

 private:
  scoped_ptr<ui::SimpleMenuModel> menu_model_;
  scoped_ptr<views::MenuModelAdapter> menu_adapter_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMenuDelegate);
};

}  // namespace

TEST_F(AccessibilityEventRouterViewsTest, MenuIndexAndCountForInvisibleMenu) {
  SimpleMenuDelegate menu_delegate;
  views::MenuItemView* menu = menu_delegate.BuildMenu();
  views::View* menu_container = menu->CreateSubmenu();

  struct TestCase {
    int command_id;
    int expected_index;
    int expected_count;
  } kTestCases[] = {
    { SimpleMenuDelegate::IDC_MENU_ITEM_1, 0, 3 },
    { SimpleMenuDelegate::IDC_MENU_ITEM_2, 1, 3 },
    { SimpleMenuDelegate::IDC_MENU_INVISIBLE, 0, 3 },
    { SimpleMenuDelegate::IDC_MENU_ITEM_3, 2, 3 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    int index = 0;
    int count = 0;

    AccessibilityEventRouterViews::RecursiveGetMenuItemIndexAndCount(
        menu_container,
        menu->GetMenuItemByID(kTestCases[i].command_id),
        &index,
        &count);
    EXPECT_EQ(kTestCases[i].expected_index, index) << "Case " << i;
    EXPECT_EQ(kTestCases[i].expected_count, count) << "Case " << i;
  }
}
