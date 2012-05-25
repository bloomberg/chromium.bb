// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_modality_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/capture_tracking_view.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

typedef test::AshTestBase WindowModalityControllerTest;

namespace {

bool ValidateStacking(aura::Window* parent, int ids[], int count) {
  for (int i = 0; i < count; ++i) {
    if (parent->children().at(i)->id() != ids[i])
      return false;
  }
  return true;
}

}  // namespace

// Creates three windows, w1, w11, and w12. w11 is a non-modal transient, w12 is
// a modal transient.
// Validates:
// - it should be possible to activate w12 even when w11 is open.
// - activating w1 activates w12 and updates stacking order appropriately.
// - closing a window passes focus up the stack.
TEST_F(WindowModalityControllerTest, BasicActivation) {
  aura::test::TestWindowDelegate d;
  scoped_ptr<aura::Window> w1(
      aura::test::CreateTestWindowWithDelegate(&d, -1, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w11(
      aura::test::CreateTestWindowWithDelegate(&d, -11, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w12(
      aura::test::CreateTestWindowWithDelegate(&d, -12, gfx::Rect(), NULL));

  w1->AddTransientChild(w11.get());
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  wm::ActivateWindow(w11.get());
  EXPECT_TRUE(wm::IsActiveWindow(w11.get()));

  w12->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  w1->AddTransientChild(w12.get());
  wm::ActivateWindow(w12.get());
  EXPECT_TRUE(wm::IsActiveWindow(w12.get()));

  wm::ActivateWindow(w11.get());
  EXPECT_TRUE(wm::IsActiveWindow(w11.get()));

  int check1[] = { -1, -12, -11 };
  EXPECT_TRUE(ValidateStacking(w1->parent(), check1, arraysize(check1)));

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w12.get()));
  // Transient children are always stacked above their transient parent, which
  // is why this order is not -11, -1, -12.
  int check2[] = { -1, -11, -12 };
  EXPECT_TRUE(ValidateStacking(w1->parent(), check2, arraysize(check2)));

  w12.reset();
  EXPECT_TRUE(wm::IsActiveWindow(w11.get()));
  w11.reset();
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// Create two toplevel windows w1 and w2, and nest two modals w11 and w111 below
// w1.
// Validates:
// - activating w1 while w11/w111 is showing always activates most deeply nested
//   descendant.
// - closing a window passes focus up the stack.
TEST_F(WindowModalityControllerTest, NestedModals) {
  aura::test::TestWindowDelegate d;
  scoped_ptr<aura::Window> w1(
      aura::test::CreateTestWindowWithDelegate(&d, -1, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w11(
      aura::test::CreateTestWindowWithDelegate(&d, -11, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w111(
      aura::test::CreateTestWindowWithDelegate(&d, -111, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w2(
      aura::test::CreateTestWindowWithDelegate(&d, -2, gfx::Rect(), NULL));

  w1->AddTransientChild(w11.get());
  w11->AddTransientChild(w111.get());

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  // Set up modality.
  w11->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  w111->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w111.get()));
  int check1[] = { -2, -1, -11, -111 };
  EXPECT_TRUE(ValidateStacking(w1->parent(), check1, arraysize(check1)));

  wm::ActivateWindow(w11.get());
  EXPECT_TRUE(wm::IsActiveWindow(w111.get()));
  EXPECT_TRUE(ValidateStacking(w1->parent(), check1, arraysize(check1)));

  wm::ActivateWindow(w111.get());
  EXPECT_TRUE(wm::IsActiveWindow(w111.get()));
  EXPECT_TRUE(ValidateStacking(w1->parent(), check1, arraysize(check1)));

  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  int check2[] = { -1, -11, -111, -2 };
  EXPECT_TRUE(ValidateStacking(w1->parent(), check2, arraysize(check2)));

  w2.reset();
  EXPECT_TRUE(wm::IsActiveWindow(w111.get()));
  w111.reset();
  EXPECT_TRUE(wm::IsActiveWindow(w11.get()));
  w11.reset();
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// Create two toplevel windows w1 and w2, and nest two modals w11 and w111 below
// w1.
// Validates:
// - destroying w11 while w111 is focused activates w1.
TEST_F(WindowModalityControllerTest, NestedModalsOuterClosed) {
  aura::test::TestWindowDelegate d;
  scoped_ptr<aura::Window> w1(
      aura::test::CreateTestWindowWithDelegate(&d, -1, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w11(
      aura::test::CreateTestWindowWithDelegate(&d, -11, gfx::Rect(), NULL));
  // |w111| will be owned and deleted by |w11|.
  aura::Window* w111 =
      aura::test::CreateTestWindowWithDelegate(&d, -111, gfx::Rect(), NULL);
  scoped_ptr<aura::Window> w2(
      aura::test::CreateTestWindowWithDelegate(&d, -2, gfx::Rect(), NULL));

  w1->AddTransientChild(w11.get());
  w11->AddTransientChild(w111);

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  // Set up modality.
  w11->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  w111->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w111));

  w111->Hide();
  EXPECT_TRUE(wm::IsActiveWindow(w11.get()));

  // TODO(oshima): Re-showing doesn't set the focus back to
  // modal window. There is no such use case right now, but it
  // probably should.

  w11.reset();
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// Modality also prevents events from being passed to the transient parent.
TEST_F(WindowModalityControllerTest, Events) {
  aura::test::TestWindowDelegate d;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(&d, -1,
      gfx::Rect(0, 0, 100, 100), NULL));
  scoped_ptr<aura::Window> w11(aura::test::CreateTestWindowWithDelegate(&d, -11,
      gfx::Rect(20, 20, 50, 50), NULL));

  w1->AddTransientChild(w11.get());

  {
    // Clicking a point within w1 should activate that window.
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         gfx::Point(10, 10));
    generator.ClickLeftButton();
    EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  }

  w11->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);

  {
    // Clicking a point within w1 should activate w11.
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         gfx::Point(10, 10));
    generator.ClickLeftButton();
    EXPECT_TRUE(wm::IsActiveWindow(w11.get()));
  }
}

// Creates windows w1 and non activatiable child w11. Creates transient window
// w2 and adds it as a transeint child of w1. Ensures that w2 is parented to
// the parent of w1, and that GetWindowModalTransient(w11) returns w2.
TEST_F(WindowModalityControllerTest, GetWindowModalTransient) {
  aura::test::TestWindowDelegate d;
  scoped_ptr<aura::Window> w1(
      aura::test::CreateTestWindowWithDelegate(&d, -1, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w11(
      aura::test::CreateTestWindowWithDelegate(&d, -11, gfx::Rect(), w1.get()));
  scoped_ptr<aura::Window> w2(
      aura::test::CreateTestWindowWithDelegate(&d, -2, gfx::Rect(), NULL));
  w2->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);

  aura::Window* wt;
  wt = wm::GetWindowModalTransient(w1.get());
  ASSERT_EQ(static_cast<aura::Window*>(NULL), wt);

  // Parent w2 to w1. It should get parented to the parent of w1.
  w1->AddTransientChild(w2.get());
  ASSERT_EQ(2U, w1->parent()->children().size());
  EXPECT_EQ(-2, w1->parent()->children().at(1)->id());

  // Request the modal transient window for w1, it should be w2.
  wt = wm::GetWindowModalTransient(w1.get());
  ASSERT_NE(static_cast<aura::Window*>(NULL), wt);
  EXPECT_EQ(-2, wt->id());

  // Request the modal transient window for w11, it should also be w2.
  wt = wm::GetWindowModalTransient(w11.get());
  ASSERT_NE(static_cast<aura::Window*>(NULL), wt);
  EXPECT_EQ(-2, wt->id());
}

// Verifies we generate a capture lost when showing a modal window.
TEST_F(WindowModalityControllerTest, ChangeCapture) {
  views::Widget* widget = views::Widget::CreateWindow(NULL);
  scoped_ptr<aura::Window> widget_window(widget->GetNativeView());
  test::CaptureTrackingView* view = new test::CaptureTrackingView;
  widget->client_view()->AddChildView(view);
  widget->SetBounds(gfx::Rect(0, 0, 200, 200));
  view->SetBoundsRect(widget->client_view()->GetLocalBounds());
  widget->Show();

  gfx::Point center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &center);
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), center);
  generator.PressLeftButton();
  EXPECT_TRUE(view->got_press());

  views::Widget* modal_widget =
      views::Widget::CreateWindowWithParent(NULL, widget->GetNativeView());
  scoped_ptr<aura::Window> modal_window(modal_widget->GetNativeView());
  modal_window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  test::CaptureTrackingView* modal_view = new test::CaptureTrackingView;
  modal_widget->client_view()->AddChildView(modal_view);
  modal_widget->SetBounds(gfx::Rect(50, 50, 200, 200));
  modal_view->SetBoundsRect(modal_widget->client_view()->GetLocalBounds());
  modal_widget->Show();

  EXPECT_TRUE(view->got_capture_lost());
  generator.ReleaseLeftButton();

  view->reset();

  EXPECT_FALSE(modal_view->got_capture_lost());
  EXPECT_FALSE(modal_view->got_press());

  gfx::Point modal_center(modal_view->width() / 2, modal_view->height() / 2);
  views::View::ConvertPointToScreen(modal_view, &modal_center);
  generator.MoveMouseTo(modal_center, 1);
  generator.PressLeftButton();
  EXPECT_TRUE(modal_view->got_press());
  EXPECT_FALSE(modal_view->got_capture_lost());
  EXPECT_FALSE(view->got_capture_lost());
  EXPECT_FALSE(view->got_press());
}


}  // namespace internal
}  // namespace ash
