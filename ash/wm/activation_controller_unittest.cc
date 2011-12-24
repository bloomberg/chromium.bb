// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/activation_controller.h"

#include "ash/test/aura_shell_test_base.h"
#include "ash/test/test_activation_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/test_window_delegate.h"

#if defined(OS_WIN)
// Windows headers define macros for these function names which screw with us.
#if defined(CreateWindow)
#undef CreateWindow
#endif
#endif

namespace ash {
namespace test {

typedef test::AuraShellTestBase ActivationControllerTest;

// Utilities for a set of tests that test
// ActivationController::GetTopmostWindowToActivate().
class GetTopmostWindowToActivateTest : public ActivationControllerTest {
 public:
  GetTopmostWindowToActivateTest() : ad_1_(false), ad_3_(false) {}
  virtual ~GetTopmostWindowToActivateTest() {}

  // Overridden from ActivationControllerTest:
  virtual void SetUp() OVERRIDE {
    ActivationControllerTest::SetUp();
    CreateWindows();
  }
  virtual void TearDown() OVERRIDE {
    DestroyWindows();
    ActivationControllerTest::TearDown();
  }

 protected:
  aura::Window* w1() { return w1_.get(); }
  aura::Window* w2() { return w2_.get(); }
  aura::Window* w3() { return w3_.get(); }
  aura::Window* w4() { return w4_.get(); }

  void DestroyWindow2() {
    w2_.reset();
  }

 private:
  void CreateWindows() {
    // Create four windows, the first and third are not activatable, the second
    // and fourth are.
    w1_.reset(CreateWindow(1, &ad_1_));
    w2_.reset(CreateWindow(2, &ad_2_));
    w3_.reset(CreateWindow(3, &ad_3_));
    w4_.reset(CreateWindow(4, &ad_4_));
  }

  aura::Window* CreateWindow(int id, TestActivationDelegate* delegate) {
    aura::Window* window = aura::test::CreateTestWindowWithDelegate(
        &delegate_, id, gfx::Rect(), NULL);
    delegate->SetWindow(window);
    return window;
  }

  void DestroyWindows() {
    w1_.reset();
    w2_.reset();
    w3_.reset();
    w4_.reset();
  }

  aura::test::TestWindowDelegate delegate_;
  TestActivationDelegate ad_1_;
  TestActivationDelegate ad_2_;
  TestActivationDelegate ad_3_;
  TestActivationDelegate ad_4_;
  scoped_ptr<aura::Window> w1_;  // Non-activatable.
  scoped_ptr<aura::Window> w2_;  // Activatable.
  scoped_ptr<aura::Window> w3_;  // Non-activatable.
  scoped_ptr<aura::Window> w4_;  // Activatable.

  DISALLOW_COPY_AND_ASSIGN(GetTopmostWindowToActivateTest);
};

// Hiding the active window should activate the next valid activatable window.
TEST_F(GetTopmostWindowToActivateTest, HideActivatesNext) {
  ActivateWindow(w2());
  EXPECT_TRUE(IsActiveWindow(w2()));

  w2()->Hide();
  EXPECT_TRUE(IsActiveWindow(w4()));
}

// Destroying the active window should activate the next valid activatable
// window.
TEST_F(GetTopmostWindowToActivateTest, DestroyActivatesNext) {
  ActivateWindow(w2());
  EXPECT_TRUE(IsActiveWindow(w2()));

  DestroyWindow2();
  EXPECT_EQ(NULL, w2());
  EXPECT_TRUE(IsActiveWindow(w4()));
}

// Deactivating the active window should activate the next valid activatable
// window.
TEST_F(GetTopmostWindowToActivateTest, DeactivateActivatesNext) {
  ActivateWindow(w2());
  EXPECT_TRUE(IsActiveWindow(w2()));

  DeactivateWindow(w2());
  EXPECT_TRUE(IsActiveWindow(w4()));
}

// Test if the clicking on a menu picks the transient parent as activatable
// window.
TEST_F(ActivationControllerTest, ClickOnMenu) {
  aura::test::TestWindowDelegate wd;
  TestActivationDelegate ad1;
  TestActivationDelegate ad2(false);

  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &wd, 1, gfx::Rect(100, 100), NULL));
  ad1.SetWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(NULL));

  // Clicking on an activatable window activtes the window.
  aura::test::EventGenerator generator(w1.get());
  generator.ClickLeftButton();
  EXPECT_TRUE(IsActiveWindow(w1.get()));

  // Creates a menu that covers the transient parent.
  scoped_ptr<aura::Window> menu(aura::test::CreateTestWindowWithDelegateAndType(
      &wd, aura::client::WINDOW_TYPE_MENU, 2, gfx::Rect(100, 100), NULL));
  ad2.SetWindow(menu.get());
  w1->AddTransientChild(menu.get());

  // Clicking on a menu whose transient parent is active window shouldn't
  // change the active window.
  generator.ClickLeftButton();
  EXPECT_TRUE(IsActiveWindow(w1.get()));
}

// Various assertions for activating/deactivating.
TEST_F(ActivationControllerTest, Deactivate) {
  aura::test::TestWindowDelegate d1;
  aura::test::TestWindowDelegate d2;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &d2, 2, gfx::Rect(), NULL));
  aura::Window* parent = w1->parent();
  parent->Show();
  ASSERT_TRUE(parent);
  ASSERT_EQ(2u, parent->children().size());
  // Activate w2 and make sure it's active and frontmost.
  ActivateWindow(w2.get());
  EXPECT_TRUE(IsActiveWindow(w2.get()));
  EXPECT_FALSE(IsActiveWindow(w1.get()));
  EXPECT_EQ(w2.get(), parent->children()[1]);

  // Activate w1 and make sure it's active and frontmost.
  ActivateWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(w1.get()));
  EXPECT_FALSE(IsActiveWindow(w2.get()));
  EXPECT_EQ(w1.get(), parent->children()[1]);

  // Deactivate w1 and make sure w2 becomes active and frontmost.
  DeactivateWindow(w1.get());
  EXPECT_FALSE(IsActiveWindow(w1.get()));
  EXPECT_TRUE(IsActiveWindow(w2.get()));
  EXPECT_EQ(w2.get(), parent->children()[1]);
}

// Verifies that when WindowDelegate::OnLostActive is invoked the window is not
// active.
TEST_F(ActivationControllerTest, NotActiveInLostActive) {
  TestActivationDelegate ad1;
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &wd, 1, gfx::Rect(10, 10, 50, 50), NULL));
  ad1.SetWindow(w1.get());
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      NULL, 1, gfx::Rect(10, 10, 50, 50), NULL));

  // Activate w1.
  ActivateWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(w1.get()));

  // Should not have gotten a OnLostActive yet.
  EXPECT_EQ(0, ad1.lost_active_count());

  // ActivateWindow(NULL) should not change the active window.
  ActivateWindow(NULL);
  EXPECT_TRUE(IsActiveWindow(w1.get()));

  // Now activate another window.
  ActivateWindow(w2.get());

  // Should have gotten OnLostActive and w1 should not have been active at that
  // time.
  EXPECT_EQ(1, ad1.lost_active_count());
  EXPECT_FALSE(ad1.window_was_active());
}

// Verifies that focusing another window or its children causes it to become
// active.
TEST_F(ActivationControllerTest, FocusTriggersActivation) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &wd, -1, gfx::Rect(50, 50), NULL));
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &wd, -2, gfx::Rect(50, 50), NULL));
  scoped_ptr<aura::Window> w21(aura::test::CreateTestWindowWithDelegate(
      &wd, -21, gfx::Rect(50, 50), w2.get()));

  ActivateWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  w2->Focus();
  EXPECT_TRUE(IsActiveWindow(w2.get()));
  EXPECT_TRUE(w2->HasFocus());

  ActivateWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  w21->Focus();
  EXPECT_TRUE(IsActiveWindow(w2.get()));
  EXPECT_TRUE(w21->HasFocus());
}

// Verifies that we prevent all attempts to focus a child of a non-activatable
// window from claiming focus to that window.
TEST_F(ActivationControllerTest, PreventFocusToNonActivatableWindow) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &wd, -1, gfx::Rect(50, 50), NULL));
  // The RootWindow itself is a non-activatable parent.
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &wd, -2, gfx::Rect(50, 50), aura::RootWindow::GetInstance()));
  scoped_ptr<aura::Window> w21(aura::test::CreateTestWindowWithDelegate(
      &wd, -21, gfx::Rect(50, 50), w2.get()));

  ActivateWindow(w1.get());
  EXPECT_TRUE(IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  // Try activating |w2|. It's not a child of an activatable container, so it
  // should neither be activated nor get focus.
  ActivateWindow(w2.get());
  EXPECT_FALSE(IsActiveWindow(w2.get()));
  EXPECT_FALSE(w2->HasFocus());
  EXPECT_TRUE(IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  // Try focusing |w2|. Same rules apply.
  w2->Focus();
  EXPECT_FALSE(IsActiveWindow(w2.get()));
  EXPECT_FALSE(w2->HasFocus());
  EXPECT_TRUE(IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  // Try focusing |w21|. Same rules apply.
  EXPECT_FALSE(IsActiveWindow(w2.get()));
  EXPECT_FALSE(w21->HasFocus());
  EXPECT_TRUE(IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());
}

}  // namespace test
}  // namespace ash
