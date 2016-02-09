// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_web_contents_factory.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"

namespace {

// A test delegate for a toolbar action view.
class TestToolbarActionViewDelegate : public ToolbarActionView::Delegate {
 public:
  TestToolbarActionViewDelegate() : shown_in_menu_(false),
                                    overflow_reference_view_(nullptr),
                                    web_contents_(nullptr) {}
  ~TestToolbarActionViewDelegate() override {}

  // ToolbarActionView::Delegate:
  content::WebContents* GetCurrentWebContents() override {
    return web_contents_;
  }
  bool ShownInsideMenu() const override { return shown_in_menu_; }
  void OnToolbarActionViewDragDone() override {}
  views::MenuButton* GetOverflowReferenceView() override {
    return overflow_reference_view_;
  }
  void OnMouseEnteredToolbarActionView() override {}
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override {}
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& p) override {
    return ui::DragDropTypes::DRAG_NONE;
  }
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override { return false; }

  void set_shown_in_menu(bool shown_in_menu) { shown_in_menu_ = shown_in_menu; }
  void set_overflow_reference_view(views::MenuButton* overflow_reference_view) {
    overflow_reference_view_ = overflow_reference_view;
  }
  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  bool shown_in_menu_;

  views::MenuButton* overflow_reference_view_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarActionViewDelegate);
};

class OpenMenuListener : public views::ContextMenuController {
 public:
  explicit OpenMenuListener(views::View* view)
      : view_(view),
        opened_menu_(false) {
    view_->set_context_menu_controller(this);
  }
  ~OpenMenuListener() override {
    view_->set_context_menu_controller(nullptr);
  }

  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override {
    opened_menu_ = true;
  };

  bool opened_menu() const { return opened_menu_; }

 private:
  views::View* view_;

  bool opened_menu_;

  DISALLOW_COPY_AND_ASSIGN(OpenMenuListener);
};

}  // namespace

class ToolbarActionViewUnitTest : public views::ViewsTestBase {
 public:
  ToolbarActionViewUnitTest()
      : widget_(nullptr),
        ui_thread_(content::BrowserThread::UI, message_loop()) {}
  ~ToolbarActionViewUnitTest() override {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    widget_ = new views::Widget;
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(params);
  }
  void TearDown() override {
    if (!widget_->IsClosed())
      widget_->Close();
    views::ViewsTestBase::TearDown();
  }

  views::Widget* widget() { return widget_; }

 private:
  // The widget managed by this test.
  views::Widget* widget_;

  // Web contents need a fake ui thread.
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionViewUnitTest);
};

// Test the basic ui of a ToolbarActionView and that it responds correctly to
// a controller's state.
TEST_F(ToolbarActionViewUnitTest, BasicToolbarActionViewTest) {
  TestingProfile profile;

  // ViewsTestBase initializees the aura environment, so the factory shouldn't.
  content::TestWebContentsFactory web_contents_factory;

  TestToolbarActionViewController controller("fake controller");
  TestToolbarActionViewDelegate action_view_delegate;

  // Configure the test controller and delegate.
  base::string16 name = base::ASCIIToUTF16("name");
  controller.SetAccessibleName(name);
  base::string16 tooltip = base::ASCIIToUTF16("tooltip");
  controller.SetTooltip(tooltip);
  content::WebContents* web_contents =
      web_contents_factory.CreateWebContents(&profile);
  SessionTabHelper::CreateForWebContents(web_contents);
  action_view_delegate.set_web_contents(web_contents);

  // Move the mouse off the not-yet-existent button.
  ui::test::EventGenerator generator(GetContext(), widget()->GetNativeWindow());
  generator.MoveMouseTo(gfx::Point(300, 300));

  // Create a new toolbar action view.
  ToolbarActionView view(&controller, &action_view_delegate);
  view.set_owned_by_client();
  view.SetBoundsRect(gfx::Rect(0, 0, 200, 20));
  widget()->SetContentsView(&view);
  widget()->Show();

  // Check that the tooltip and accessible state of the view match the
  // controller's.
  base::string16 tooltip_test;
  EXPECT_TRUE(view.GetTooltipText(gfx::Point(), &tooltip_test));
  EXPECT_EQ(tooltip, tooltip_test);
  ui::AXViewState ax_state;
  view.GetAccessibleState(&ax_state);
  EXPECT_EQ(name, ax_state.name);

  // The button should start in normal state, with no actions executed.
  EXPECT_EQ(views::Button::STATE_NORMAL, view.state());
  EXPECT_EQ(0, controller.execute_action_count());

  // Click the button. This should execute it.
  generator.MoveMouseTo(gfx::Point(10, 10));
  generator.ClickLeftButton();
  EXPECT_EQ(1, controller.execute_action_count());

  // Move the mouse off the button, and show a popup through a non-user action.
  // Since this was not a user action, the button should not be pressed.
  generator.MoveMouseTo(gfx::Point(300, 300));
  controller.ShowPopup(false);
  EXPECT_EQ(views::Button::STATE_NORMAL, view.state());
  controller.HidePopup();

  // Show the popup through a user action - the button should be pressed.
  controller.ShowPopup(true);
  EXPECT_EQ(views::Button::STATE_PRESSED, view.state());
  controller.HidePopup();
  EXPECT_EQ(views::Button::STATE_NORMAL, view.state());

  // Ensure that the button's enabled state reflects that of the controller.
  controller.SetEnabled(false);
  EXPECT_EQ(views::Button::STATE_DISABLED, view.state());
  controller.SetEnabled(true);
  EXPECT_EQ(views::Button::STATE_NORMAL, view.state());

  // Ensure that clicking on an otherwise-disabled action optionally opens the
  // context menu.
  controller.SetDisabledClickOpensMenu(true);
  controller.SetEnabled(false);
  EXPECT_EQ(views::Button::STATE_NORMAL, view.state());
  int old_execute_action_count = controller.execute_action_count();
  {
    OpenMenuListener menu_listener(&view);
    view.Activate();
    EXPECT_TRUE(menu_listener.opened_menu());
    EXPECT_EQ(old_execute_action_count, controller.execute_action_count());
  }

  // Ensure that the button's want-to-run state reflects that of the controller.
  controller.SetWantsToRun(true);
  EXPECT_TRUE(view.wants_to_run_for_testing());
  controller.SetWantsToRun(false);
  EXPECT_FALSE(view.wants_to_run_for_testing());

  // Create an overflow button.
  views::MenuButton overflow_button(base::string16(), nullptr, false);
  overflow_button.set_owned_by_client();
  action_view_delegate.set_overflow_reference_view(&overflow_button);

  // If the view isn't visible, the overflow button should be pressed for
  // popups.
  view.SetVisible(false);
  controller.ShowPopup(true);
  EXPECT_EQ(views::Button::STATE_NORMAL, view.state());
  EXPECT_EQ(views::Button::STATE_PRESSED, overflow_button.state());
  controller.HidePopup();
  EXPECT_EQ(views::Button::STATE_NORMAL, view.state());
  EXPECT_EQ(views::Button::STATE_NORMAL, overflow_button.state());
}
