// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_modality_controller.h"

#include "ash/test/aura_shell_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {
namespace internal {

typedef test::AuraShellTestBase WindowModalityControllerTest;

namespace {

bool ValidateStacking(aura::Window* parent, int ids[], int count) {
  for (int i = 0; i < count; ++i) {
    if (parent->children().at(i)->id() != ids[i])
      return false;
  }
  return true;
}

}

// Creates three windows, w1, w11, and w12. w11 is a non-modal transient, w12 is
// a modal transient.
// Validates:
// - it should be possible to activate w12 even when w11 is open.
// - activating w1 activates w12 and updates stacking order appropriately.
TEST_F(WindowModalityControllerTest, BasicActivation) {
  aura::test::TestWindowDelegate d;
  scoped_ptr<aura::Window> w1(
      aura::test::CreateTestWindowWithDelegate(&d, -1, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w11(
      aura::test::CreateTestWindowWithDelegate(&d, -11, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w12(
      aura::test::CreateTestWindowWithDelegate(&d, -12, gfx::Rect(), NULL));

  w1->AddTransientChild(w11.get());
  ActivateWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(w1.get()));
  ActivateWindow(w11.get());
  EXPECT_TRUE(IsActiveWindow(w11.get()));

  w12->SetIntProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  w1->AddTransientChild(w12.get());
  ActivateWindow(w12.get());
  EXPECT_TRUE(IsActiveWindow(w12.get()));

  ActivateWindow(w11.get());
  EXPECT_TRUE(IsActiveWindow(w11.get()));

  int check1[] = { -1, -12, -11 };
  EXPECT_TRUE(ValidateStacking(w1->parent(), check1, arraysize(check1)));

  ActivateWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(w12.get()));
  // Transient children are always stacked above their transient parent, which
  // is why this order is not -11, -1, -12.
  int check2[] = { -1, -11, -12 };
  EXPECT_TRUE(ValidateStacking(w1->parent(), check2, arraysize(check2)));
}

// Create two toplevel windows w1 and w2, and nest two modals w11 and w111 below
// w1.
// Validates:
// - activating w1 while w11/w111 is showing always activates most deeply nested
//   descendant.
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

  ActivateWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(w1.get()));
  ActivateWindow(w2.get());
  EXPECT_TRUE(IsActiveWindow(w2.get()));

  // Set up modality.
  w11->SetIntProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  w111->SetIntProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);

  ActivateWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(w111.get()));
  int check1[] = { -2, -1, -11, -111 };
  EXPECT_TRUE(ValidateStacking(w1->parent(), check1, arraysize(check1)));

  ActivateWindow(w11.get());
  EXPECT_TRUE(IsActiveWindow(w111.get()));
  EXPECT_TRUE(ValidateStacking(w1->parent(), check1, arraysize(check1)));

  ActivateWindow(w111.get());
  EXPECT_TRUE(IsActiveWindow(w111.get()));
  EXPECT_TRUE(ValidateStacking(w1->parent(), check1, arraysize(check1)));

  ActivateWindow(w2.get());
  EXPECT_TRUE(IsActiveWindow(w2.get()));
  int check2[] = { -1, -11, -111, -2 };
  EXPECT_TRUE(ValidateStacking(w1->parent(), check2, arraysize(check2)));
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
    aura::test::EventGenerator generator(gfx::Point(10, 10));
    generator.ClickLeftButton();
    EXPECT_TRUE(IsActiveWindow(w1.get()));
  }

  w11->SetIntProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);

  {
    // Clicking a point within w1 should activate w11.
    aura::test::EventGenerator generator(gfx::Point(10, 10));
    generator.ClickLeftButton();
    EXPECT_TRUE(IsActiveWindow(w11.get()));
  }
}


}  // namespace internal
}  // namespace ash