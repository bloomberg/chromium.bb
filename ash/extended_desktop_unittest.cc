// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/monitor_controller.h"
#include "ash/monitor/multi_monitor_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/display.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

views::Widget* CreateTestWidget(const gfx::Rect& bounds) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = bounds;
  views::Widget* widget = new views::Widget;
  widget->Init(params);
  widget->Show();
  return widget;
}

class ModalWidgetDelegate : public views::WidgetDelegateView {
 public:
  ModalWidgetDelegate() {}
  virtual ~ModalWidgetDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_SYSTEM;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalWidgetDelegate);
};

}  // namespace

class ExtendedDesktopTest : public test::AshTestBase {
 public:
  ExtendedDesktopTest() {}
  virtual ~ExtendedDesktopTest() {}

  virtual void SetUp() OVERRIDE {
    internal::MonitorController::SetExtendedDesktopEnabled(true);
    AshTestBase::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    AshTestBase::TearDown();
    internal::MonitorController::SetExtendedDesktopEnabled(false);
  }

 protected:
  internal::MultiMonitorManager* monitor_manager() {
    return static_cast<internal::MultiMonitorManager*>(
        aura::Env::GetInstance()->monitor_manager());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtendedDesktopTest);
};

// Test conditions that root windows in extended desktop mode
// must satisfy.
TEST_F(ExtendedDesktopTest, Basic) {
  UpdateMonitor("0+0-1000x600,1001+0-600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  // All root windows must have the root window controller.
  ASSERT_EQ(2U, root_windows.size());
  for (Shell::RootWindowList::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    EXPECT_TRUE(wm::GetRootWindowController(*iter) != NULL);
  }
  // Make sure root windows share the same controllers.
  EXPECT_EQ(root_windows[0]->GetFocusManager(),
            root_windows[1]->GetFocusManager());
  EXPECT_EQ(aura::client::GetActivationClient(root_windows[0]),
            aura::client::GetActivationClient(root_windows[1]));
  EXPECT_EQ(aura::client::GetCaptureClient(root_windows[0]),
            aura::client::GetCaptureClient(root_windows[1]));
}

TEST_F(ExtendedDesktopTest, Activation) {
  UpdateMonitor("0+0-1000x600,1001+0-600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  // Move the active root window to the secondary.
  Shell::GetInstance()->set_active_root_window(root_windows[1]);

  views::Widget* widget_on_2nd = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_EQ(root_windows[1], widget_on_2nd->GetNativeView()->GetRootWindow());

  // Move the active root window back to the primary.
  Shell::GetInstance()->set_active_root_window(root_windows[0]);

  views::Widget* widget_on_1st = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_EQ(root_windows[0], widget_on_1st->GetNativeView()->GetRootWindow());

  aura::test::EventGenerator generator_1st(root_windows[0]);
  aura::test::EventGenerator generator_2nd(root_windows[1]);

  // Clicking a window changes the active window and active root window.
  generator_2nd.MoveMouseToCenterOf(widget_on_2nd->GetNativeView());
  generator_2nd.ClickLeftButton();

  EXPECT_EQ(widget_on_2nd->GetNativeView(),
            root_windows[0]->GetFocusManager()->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_2nd->GetNativeView()));

  generator_1st.MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  generator_1st.ClickLeftButton();

  EXPECT_EQ(widget_on_1st->GetNativeView(),
            root_windows[0]->GetFocusManager()->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));
}

TEST_F(ExtendedDesktopTest, SystemModal) {
  UpdateMonitor("0+0-1000x600,1001+0-600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  Shell::GetInstance()->set_active_root_window(root_windows[0]);

  views::Widget* widget_on_1st = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));
  EXPECT_EQ(root_windows[0], Shell::GetActiveRootWindow());

  // Change the active root window to 2nd.
  Shell::GetInstance()->set_active_root_window(root_windows[1]);

  // Open system modal. Make sure it's on 2nd root window and active.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWidgetDelegate(), NULL);
  modal_widget->Show();
  EXPECT_TRUE(wm::IsActiveWindow(modal_widget->GetNativeView()));
  EXPECT_EQ(root_windows[1], modal_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], Shell::GetActiveRootWindow());

  // Clicking a widget on widget_on_1st monitor should not change activation.
  aura::test::EventGenerator generator_1st(root_windows[0]);
  generator_1st.MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal_widget->GetNativeView()));
  EXPECT_EQ(root_windows[1], Shell::GetActiveRootWindow());

  // Close system modal and so clicking a widget should work now.
  modal_widget->Close();
  generator_1st.MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));
  EXPECT_EQ(root_windows[0], Shell::GetActiveRootWindow());
}

TEST_F(ExtendedDesktopTest, TestCursor) {
  UpdateMonitor("0+0-1000x600,1001+0-600x400");
  Shell::GetInstance()->ShowCursor(false);
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  EXPECT_FALSE(root_windows[0]->cursor_shown());
  EXPECT_FALSE(root_windows[1]->cursor_shown());
  Shell::GetInstance()->ShowCursor(true);
  EXPECT_TRUE(root_windows[0]->cursor_shown());
  EXPECT_TRUE(root_windows[1]->cursor_shown());

  EXPECT_EQ(ui::kCursorPointer, root_windows[0]->last_cursor().native_type());
  EXPECT_EQ(ui::kCursorPointer, root_windows[1]->last_cursor().native_type());
  Shell::GetInstance()->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, root_windows[0]->last_cursor().native_type());
  EXPECT_EQ(ui::kCursorCopy, root_windows[1]->last_cursor().native_type());
}

TEST_F(ExtendedDesktopTest, CycleWindows) {
  UpdateMonitor("0+0-1000x600,1001+0-600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Switch active windows between root windows.
  Shell::GetInstance()->set_active_root_window(root_windows[0]);
  views::Widget* d1_w1 = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_EQ(root_windows[0], d1_w1->GetNativeView()->GetRootWindow());

  Shell::GetInstance()->set_active_root_window(root_windows[1]);
  views::Widget* d2_w1 = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_EQ(root_windows[1], d2_w1->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));

  // Cycle through all windows across root windows.
  Shell::GetInstance()->set_active_root_window(root_windows[0]);
  views::Widget* d1_w2 = CreateTestWidget(gfx::Rect(100, 100, 100, 100));
  EXPECT_EQ(root_windows[0], d1_w2->GetNativeView()->GetRootWindow());

  Shell::GetInstance()->set_active_root_window(root_windows[1]);
  views::Widget* d2_w2 = CreateTestWidget(gfx::Rect(100, 100, 100, 100));
  EXPECT_EQ(root_windows[1], d2_w2->GetNativeView()->GetRootWindow());

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w2->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w2->GetNativeView()));

  // Backwards
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w2->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w2->GetNativeView()));
}

TEST_F(ExtendedDesktopTest, Capture) {
  UpdateMonitor("0+0-1000x600,1001+0-600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  aura::test::EventCountDelegate r1_d1;
  aura::test::EventCountDelegate r1_d2;
  aura::test::EventCountDelegate r2_d1;

  scoped_ptr<aura::Window> r1_w1(aura::test::CreateTestWindowWithDelegate(
      &r1_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[0]));
  scoped_ptr<aura::Window> r1_w2(aura::test::CreateTestWindowWithDelegate(
      &r1_d2, 0, gfx::Rect(10, 100, 100, 100), root_windows[0]));
  scoped_ptr<aura::Window> r2_w1(aura::test::CreateTestWindowWithDelegate(
      &r2_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[1]));
  r1_w1->SetCapture();

  EXPECT_EQ(r1_w1.get(),
            aura::client::GetCaptureWindow(r2_w1->GetRootWindow()));
  aura::test::EventGenerator generator2(root_windows[1]);
  generator2.MoveMouseToCenterOf(r2_w1.get());
  generator2.ClickLeftButton();
  EXPECT_EQ("0 0 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r2_d1.GetMouseButtonCountsAndReset());
  EXPECT_EQ("1 1 0", r1_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r1_d1.GetMouseButtonCountsAndReset());

  r1_w2->SetCapture();
  EXPECT_EQ(r1_w2.get(),
            aura::client::GetCaptureWindow(r2_w1->GetRootWindow()));
  generator2.MoveMouseBy(10, 10);
  generator2.ClickLeftButton();
  EXPECT_EQ("0 0 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r2_d1.GetMouseButtonCountsAndReset());
  // mouse is already entered.
  EXPECT_EQ("0 1 0", r1_d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r1_d2.GetMouseButtonCountsAndReset());

  r1_w2->ReleaseCapture();
  EXPECT_EQ(NULL,
            aura::client::GetCaptureWindow(r2_w1->GetRootWindow()));
  generator2.MoveMouseBy(-10, -10);
  generator2.ClickLeftButton();
  EXPECT_EQ("1 1 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r2_d1.GetMouseButtonCountsAndReset());
  // Make sure the mouse_moved_handler_ is properly reset.
  EXPECT_EQ("0 0 0", r1_d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r1_d2.GetMouseButtonCountsAndReset());
}

namespace internal {
// Test if the Window::ConvertPointToWindow works across root windows.
// TODO(oshima): Move multiple display suport and this test to aura.
TEST_F(ExtendedDesktopTest, ConvertPoint) {
  UpdateMonitor("0+0-1000x600,1001+0-600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  gfx::Display& display_1 =
      monitor_manager()->FindDisplayForRootWindow(root_windows[0]);
  EXPECT_EQ("0,0", display_1.bounds().origin().ToString());
  gfx::Display& display_2 =
      monitor_manager()->FindDisplayForRootWindow(root_windows[1]);
  Shell::GetInstance()->set_active_root_window(root_windows[0]);
  aura::Window* d1 =
      CreateTestWidget(gfx::Rect(10, 10, 100, 100))->GetNativeView();
  Shell::GetInstance()->set_active_root_window(root_windows[1]);
  aura::Window* d2 =
      CreateTestWidget(gfx::Rect(20, 20, 100, 100))->GetNativeView();

  // TODO(oshima):
  // This is a hack to emulate virtual screen coordinates. Cleanup this
  // when the virtual screen coordinates is implemented.a
  gfx::Rect bounds = display_2.bounds();
  bounds.set_origin(gfx::Point(500, 500));
  display_2.set_bounds(bounds);
  // Convert point in the Root2's window to the Root1's window Coord.
  gfx::Point p(0, 0);
  aura::Window::ConvertPointToWindow(root_windows[1], root_windows[0], &p);
  EXPECT_EQ("500,500", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToWindow(d2, d1, &p);
  EXPECT_EQ("510,510", p.ToString());

  // Convert point in the Root1's window to the Root2's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToWindow(root_windows[0], root_windows[1], &p);
  EXPECT_EQ("-500,-500", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToWindow(d1, d2, &p);
  EXPECT_EQ("-510,-510", p.ToString());
}
}  // namespace internal
}  // namespace ash
