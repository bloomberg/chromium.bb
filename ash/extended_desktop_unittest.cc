// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"
#include "ash/display/multi_display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/property_util.h"
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
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

views::Widget* CreateTestWidgetWithParent(views::Widget* parent,
                                          const gfx::Rect& bounds,
                                          bool child) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.parent_widget = parent;
  params.bounds = bounds;
  params.child = child;
  views::Widget* widget = new views::Widget;
  widget->Init(params);
  widget->Show();
  return widget;
}

views::Widget* CreateTestWidget(const gfx::Rect& bounds) {
  return CreateTestWidgetWithParent(NULL, bounds, false);
}

void SetSecondaryDisplayLayout(DisplayLayout::Position position) {
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  DisplayLayout layout = display_controller->default_display_layout();
  layout.position = position;
  display_controller->SetDefaultDisplayLayout(layout);
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

internal::MultiDisplayManager* GetDisplayManager() {
  return static_cast<internal::MultiDisplayManager*>(
      aura::Env::GetInstance()->display_manager());
}

}  // namespace

typedef test::AshTestBase ExtendedDesktopTest;

// Test conditions that root windows in extended desktop mode
// must satisfy.
TEST_F(ExtendedDesktopTest, Basic) {
  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  // All root windows must have the root window controller.
  ASSERT_EQ(2U, root_windows.size());
  for (Shell::RootWindowList::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    EXPECT_TRUE(GetRootWindowController(*iter) != NULL);
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
  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  views::Widget* widget_on_1st = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  views::Widget* widget_on_2nd =
      CreateTestWidget(gfx::Rect(1200, 10, 100, 100));
  EXPECT_EQ(root_windows[0], widget_on_1st->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], widget_on_2nd->GetNativeView()->GetRootWindow());

  EXPECT_EQ(widget_on_2nd->GetNativeView(),
            root_windows[0]->GetFocusManager()->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_2nd->GetNativeView()));

  aura::test::EventGenerator generator_1st(root_windows[0]);
  aura::test::EventGenerator generator_2nd(root_windows[1]);

  // Clicking a window changes the active window and active root window.
  generator_1st.MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  generator_1st.ClickLeftButton();

  EXPECT_EQ(widget_on_1st->GetNativeView(),
            root_windows[0]->GetFocusManager()->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));

  generator_2nd.MoveMouseToCenterOf(widget_on_2nd->GetNativeView());
  generator_2nd.ClickLeftButton();

  EXPECT_EQ(widget_on_2nd->GetNativeView(),
            root_windows[0]->GetFocusManager()->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_2nd->GetNativeView()));
}

TEST_F(ExtendedDesktopTest, SystemModal) {
  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  views::Widget* widget_on_1st = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));
  EXPECT_EQ(root_windows[0], Shell::GetActiveRootWindow());

  // Open system modal. Make sure it's on 2nd root window and active.
  views::Widget* modal_widget = views::Widget::CreateWindowWithBounds(
      new ModalWidgetDelegate(), gfx::Rect(1200, 100, 100, 100));
  modal_widget->Show();
  EXPECT_TRUE(wm::IsActiveWindow(modal_widget->GetNativeView()));
  EXPECT_EQ(root_windows[1], modal_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], Shell::GetActiveRootWindow());

  // Clicking a widget on widget_on_1st display should not change activation.
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
  UpdateDisplay("1000x600,600x400");
  Shell::GetInstance()->cursor_manager()->ShowCursor(false);
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  EXPECT_FALSE(root_windows[0]->cursor_shown());
  EXPECT_FALSE(root_windows[1]->cursor_shown());
  Shell::GetInstance()->cursor_manager()->ShowCursor(true);
  EXPECT_TRUE(root_windows[0]->cursor_shown());
  EXPECT_TRUE(root_windows[1]->cursor_shown());

  EXPECT_EQ(ui::kCursorPointer, root_windows[0]->last_cursor().native_type());
  EXPECT_EQ(ui::kCursorPointer, root_windows[1]->last_cursor().native_type());
  Shell::GetInstance()->cursor_manager()->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, root_windows[0]->last_cursor().native_type());
  EXPECT_EQ(ui::kCursorCopy, root_windows[1]->last_cursor().native_type());
}

TEST_F(ExtendedDesktopTest, TestCursorLocation) {
  UpdateDisplay("0+0-1000x600,1001+0-600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  aura::Window::TestApi root_window0_test_api(root_windows[0]);
  aura::Window::TestApi root_window1_test_api(root_windows[1]);

  root_windows[0]->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("10,10", gfx::Screen::GetCursorScreenPoint().ToString());
  EXPECT_TRUE(root_window0_test_api.ContainsMouse());
  EXPECT_FALSE(root_window1_test_api.ContainsMouse());
  root_windows[1]->MoveCursorTo(gfx::Point(10, 20));
  EXPECT_EQ("1010,20", gfx::Screen::GetCursorScreenPoint().ToString());
  EXPECT_FALSE(root_window0_test_api.ContainsMouse());
  EXPECT_TRUE(root_window1_test_api.ContainsMouse());
  root_windows[0]->MoveCursorTo(gfx::Point(20, 10));
  EXPECT_EQ("20,10", gfx::Screen::GetCursorScreenPoint().ToString());
  EXPECT_TRUE(root_window0_test_api.ContainsMouse());
  EXPECT_FALSE(root_window1_test_api.ContainsMouse());
}

TEST_F(ExtendedDesktopTest, CycleWindows) {
  UpdateDisplay("700x500,500x500");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  views::Widget* d1_w1 = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_EQ(root_windows[0], d1_w1->GetNativeView()->GetRootWindow());
  views::Widget* d2_w1 = CreateTestWidget(gfx::Rect(800, 10, 100, 100));
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
  views::Widget* d1_w2 = CreateTestWidget(gfx::Rect(10, 200, 100, 100));
  EXPECT_EQ(root_windows[0], d1_w2->GetNativeView()->GetRootWindow());
  views::Widget* d2_w2 = CreateTestWidget(gfx::Rect(800, 200, 100, 100));
  EXPECT_EQ(root_windows[1], d2_w2->GetNativeView()->GetRootWindow());

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w2->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w2->GetNativeView()));

  // Backwards
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w2->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w2->GetNativeView()));
}

TEST_F(ExtendedDesktopTest, GetRootWindowAt) {
  UpdateDisplay("700x500,500x500");
  SetSecondaryDisplayLayout(DisplayLayout::LEFT);
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  EXPECT_EQ(root_windows[1], wm::GetRootWindowAt(gfx::Point(-400, 100)));
  EXPECT_EQ(root_windows[1], wm::GetRootWindowAt(gfx::Point(-1, 100)));
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(0, 300)));
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(700,300)));

  // Zero origin.
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(0, 0)));

  // Out of range point should return the primary root window
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(-600, 0)));
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(701, 100)));
}

TEST_F(ExtendedDesktopTest, GetRootWindowMatching) {
  UpdateDisplay("700x500,500x500");
  SetSecondaryDisplayLayout(DisplayLayout::LEFT);

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  // Containing rect.
  EXPECT_EQ(root_windows[1],
            wm::GetRootWindowMatching(gfx::Rect(-300, 10, 50, 50)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(100, 10, 50, 50)));

  // Intersecting rect.
  EXPECT_EQ(root_windows[1],
            wm::GetRootWindowMatching(gfx::Rect(-200, 0, 300, 300)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(-100, 0, 300, 300)));

  // Zero origin.
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(0, 0, 0, 0)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(0, 0, 1, 1)));

  // Empty rect.
  EXPECT_EQ(root_windows[1],
            wm::GetRootWindowMatching(gfx::Rect(-400, 100, 0, 0)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(100, 100, 0, 0)));

  // Out of range rect should return the primary root window.
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(-600, -300, 50, 50)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(0, 1000, 50, 50)));
}

TEST_F(ExtendedDesktopTest, Capture) {
  UpdateDisplay("1000x600,600x400");
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
  // The mouse is outside. On chromeos, the mouse is warped to the
  // dest root window, but it's not implemented on Win yet, so
  // no mouse move event on Win.
#if defined(OS_WIN)
  EXPECT_EQ("1 0 0", r1_d1.GetMouseMotionCountsAndReset());
#else
  EXPECT_EQ("1 1 0", r1_d1.GetMouseMotionCountsAndReset());
#endif
  EXPECT_EQ("1 1", r1_d1.GetMouseButtonCountsAndReset());
  // (15,15) on 1st display is (-985,15) on 2nd display.
  generator2.MoveMouseTo(-985, 15);
  EXPECT_EQ("0 1 0", r1_d1.GetMouseMotionCountsAndReset());

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
  generator2.MoveMouseTo(15, 15);
  generator2.ClickLeftButton();
  EXPECT_EQ("1 1 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r2_d1.GetMouseButtonCountsAndReset());
  // Make sure the mouse_moved_handler_ is properly reset.
  EXPECT_EQ("0 0 0", r1_d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r1_d2.GetMouseButtonCountsAndReset());
}

TEST_F(ExtendedDesktopTest, MoveWindow) {
  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  views::Widget* d1 = CreateTestWidget(gfx::Rect(10, 10, 100, 100));

  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());

  d1->SetBounds(gfx::Rect(1010, 10, 100, 100));
  EXPECT_EQ("1010,10 100x100",
            d1->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[1], d1->GetNativeView()->GetRootWindow());

  d1->SetBounds(gfx::Rect(10, 10, 100, 100));
  EXPECT_EQ("10,10 100x100",
            d1->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());

  // Make sure the bounds which doesn't fit to the root window
  // works correctly.
  d1->SetBounds(gfx::Rect(1560, 30, 100, 100));
  EXPECT_EQ(root_windows[1], d1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("1560,30 100x100",
            d1->GetWindowBoundsInScreen().ToString());

  // Setting outside of root windows will be moved to primary root window.
  // TODO(oshima): This one probably should pick the closest root window.
  d1->SetBounds(gfx::Rect(200, 10, 100, 100));
  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());
}

TEST_F(ExtendedDesktopTest, MoveWindowToDisplay) {
  UpdateDisplay("1000x1000,1000x1000");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  gfx::Display display0 =
      gfx::Screen::GetDisplayMatching(root_windows[0]->GetBoundsInScreen());
  gfx::Display display1 =
      gfx::Screen::GetDisplayMatching(root_windows[1]->GetBoundsInScreen());
  EXPECT_NE(display0.id(), display1.id());

  views::Widget* d1 = CreateTestWidget(gfx::Rect(10, 10, 1000, 100));
  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());

  // Move the window where the window spans both root windows. Since the second
  // parameter is |display1|, the window should be shown on the secondary root.
  d1->GetNativeWindow()->SetBoundsInScreen(gfx::Rect(500, 10, 1000, 100),
                                           display1);
  EXPECT_EQ("500,10 1000x100",
            d1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(root_windows[1], d1->GetNativeView()->GetRootWindow());

  // Move to the primary root.
  d1->GetNativeWindow()->SetBoundsInScreen(gfx::Rect(500, 10, 1000, 100),
                                           display0);
  EXPECT_EQ("500,10 1000x100",
            d1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());
}

TEST_F(ExtendedDesktopTest, MoveWindowWithTransient) {
  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  views::Widget* w1 = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  views::Widget* w1_t1 = CreateTestWidgetWithParent(
      w1, gfx::Rect(50, 50, 50, 50), false /* transient */);
  // Transient child of the transient child.
  views::Widget* w1_t11 = CreateTestWidgetWithParent(
      w1_t1, gfx::Rect(1200, 70, 30, 30), false /* transient */);

  views::Widget* w11 = CreateTestWidgetWithParent(
      w1, gfx::Rect(10, 10, 40, 40), true /* child */);
  views::Widget* w11_t1 = CreateTestWidgetWithParent(
      w1, gfx::Rect(1300, 100, 80, 80), false /* transient */);

  EXPECT_EQ(root_windows[0], w1->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], w11->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], w1_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], w1_t11->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], w11_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("50,50 50x50",
            w1_t1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("1200,70 30x30",
            w1_t11->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("20,20 40x40",
            w11->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("1300,100 80x80",
            w11_t1->GetWindowBoundsInScreen().ToString());

  w1->SetBounds(gfx::Rect(1100,10,100,100));

  EXPECT_EQ(root_windows[1], w1_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], w1_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], w1_t11->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], w11->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], w11_t1->GetNativeView()->GetRootWindow());

  EXPECT_EQ("1110,20 40x40",
            w11->GetWindowBoundsInScreen().ToString());
  // Transient window's screen bounds stays the same.
  EXPECT_EQ("50,50 50x50",
            w1_t1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("1200,70 30x30",
            w1_t11->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("1300,100 80x80",
            w11_t1->GetWindowBoundsInScreen().ToString());

  // Transient window doesn't move between root window unless
  // its transient parent moves.
  w1_t1->SetBounds(gfx::Rect(10, 50, 50, 50));
  EXPECT_EQ(root_windows[1], w1_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("10,50 50x50",
            w1_t1->GetWindowBoundsInScreen().ToString());
}

namespace internal {
// Test if the Window::ConvertPointToTarget works across root windows.
// TODO(oshima): Move multiple display suport and this test to aura.
TEST_F(ExtendedDesktopTest, ConvertPoint) {
  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  gfx::Display& display_1 =
      GetDisplayManager()->FindDisplayForRootWindow(root_windows[0]);
  EXPECT_EQ("0,0", display_1.bounds().origin().ToString());
  gfx::Display& display_2 =
      GetDisplayManager()->FindDisplayForRootWindow(root_windows[1]);
  EXPECT_EQ("1000,0", display_2.bounds().origin().ToString());

  aura::Window* d1 =
      CreateTestWidget(gfx::Rect(10, 10, 100, 100))->GetNativeView();
  aura::Window* d2 =
      CreateTestWidget(gfx::Rect(1020, 20, 100, 100))->GetNativeView();
  EXPECT_EQ(root_windows[0], d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], d2->GetRootWindow());

  // Convert point in the Root2's window to the Root1's window Coord.
  gfx::Point p(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[1], root_windows[0], &p);
  EXPECT_EQ("1000,0", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d2, d1, &p);
  EXPECT_EQ("1010,10", p.ToString());

  // Convert point in the Root1's window to the Root2's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[0], root_windows[1], &p);
  EXPECT_EQ("-1000,0", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d1, d2, &p);
  EXPECT_EQ("-1010,-10", p.ToString());

  // Move the 2nd display to the bottom and test again.
  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);

  display_2 = GetDisplayManager()->FindDisplayForRootWindow(root_windows[1]);
  EXPECT_EQ("0,600", display_2.bounds().origin().ToString());

  // Convert point in Root2's window to Root1's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[1], root_windows[0], &p);
  EXPECT_EQ("0,600", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d2, d1, &p);
  EXPECT_EQ("10,610", p.ToString());

  // Convert point in Root1's window to Root2's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[0], root_windows[1], &p);
  EXPECT_EQ("0,-600", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d1, d2, &p);
  EXPECT_EQ("-10,-610", p.ToString());
}

}  // namespace internal
}  // namespace ash
