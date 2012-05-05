// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/activation_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_activation_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/compositor/layer.h"

#if defined(OS_WIN)
// Windows headers define macros for these function names which screw with us.
#if defined(CreateWindow)
#undef CreateWindow
#endif
#endif

namespace {

// Containers used for the tests.
const int c1 = ash::internal::kShellWindowId_DefaultContainer;
const int c2 = ash::internal::kShellWindowId_AlwaysOnTopContainer;
const int c3 = ash::internal::kShellWindowId_LockScreenContainer;

}  // namespace

namespace ash {
namespace test {

typedef test::AshTestBase ActivationControllerTest;

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
  aura::Window* w5() { return w5_.get(); }
  aura::Window* w6() { return w6_.get(); }
  aura::Window* w7() { return w7_.get(); }

  void DestroyWindow2() {
    w2_.reset();
  }

 private:
  void CreateWindows() {
    // Create four windows, the first and third are not activatable, the second
    // and fourth are.
    w1_.reset(CreateWindow(1, &ad_1_, c1));
    w2_.reset(CreateWindow(2, &ad_2_, c1));
    w3_.reset(CreateWindow(3, &ad_3_, c1));
    w4_.reset(CreateWindow(4, &ad_4_, c1));
    w5_.reset(CreateWindow(5, &ad_5_, c2));
    w6_.reset(CreateWindow(6, &ad_6_, c2));
    w7_.reset(CreateWindow(7, &ad_7_, c3));
  }

  aura::Window* CreateWindow(int id,
                             TestActivationDelegate* delegate,
                             int container_id) {
    aura::Window* window = aura::test::CreateTestWindowWithDelegate(
        &delegate_,
        id,
        gfx::Rect(),
        Shell::GetInstance()->GetContainer(container_id));
    delegate->SetWindow(window);
    return window;
  }

  void DestroyWindows() {
    w1_.reset();
    w2_.reset();
    w3_.reset();
    w4_.reset();
    w5_.reset();
    w6_.reset();
    w7_.reset();
  }

  aura::test::TestWindowDelegate delegate_;
  TestActivationDelegate ad_1_;
  TestActivationDelegate ad_2_;
  TestActivationDelegate ad_3_;
  TestActivationDelegate ad_4_;
  TestActivationDelegate ad_5_;
  TestActivationDelegate ad_6_;
  TestActivationDelegate ad_7_;
  scoped_ptr<aura::Window> w1_;  // Non-activatable.
  scoped_ptr<aura::Window> w2_;  // Activatable.
  scoped_ptr<aura::Window> w3_;  // Non-activatable.
  scoped_ptr<aura::Window> w4_;  // Activatable.
  scoped_ptr<aura::Window> w5_;  // Activatable - Always on top.
  scoped_ptr<aura::Window> w6_;  // Activatable - Always on top.
  scoped_ptr<aura::Window> w7_;  // Activatable - Lock screen window.

  DISALLOW_COPY_AND_ASSIGN(GetTopmostWindowToActivateTest);
};

// Hiding the active window should activate the next valid activatable window.
TEST_F(GetTopmostWindowToActivateTest, HideActivatesNext) {
  wm::ActivateWindow(w2());
  EXPECT_TRUE(wm::IsActiveWindow(w2()));

  w2()->Hide();
  EXPECT_TRUE(wm::IsActiveWindow(w4()));
}

// Destroying the active window should activate the next valid activatable
// window.
TEST_F(GetTopmostWindowToActivateTest, DestroyActivatesNext) {
  wm::ActivateWindow(w2());
  EXPECT_TRUE(wm::IsActiveWindow(w2()));

  DestroyWindow2();
  EXPECT_EQ(NULL, w2());
  EXPECT_TRUE(wm::IsActiveWindow(w4()));
}

// Deactivating the active window should activate the next valid activatable
// window.
TEST_F(GetTopmostWindowToActivateTest, DeactivateActivatesNext) {
  wm::ActivateWindow(w2());
  EXPECT_TRUE(wm::IsActiveWindow(w2()));

  wm::DeactivateWindow(w2());
  EXPECT_TRUE(wm::IsActiveWindow(w4()));
}

// Test that hiding a window in a higher container will activate another window
// in that container.
TEST_F(GetTopmostWindowToActivateTest, HideActivatesSameContainer) {
  wm::ActivateWindow(w6());
  EXPECT_TRUE(wm::IsActiveWindow(w6()));

  w6()->Hide();
  EXPECT_TRUE(wm::IsActiveWindow(w5()));
}

// Test that hiding the lock window will activate a window from the next highest
// container.
TEST_F(GetTopmostWindowToActivateTest, UnlockActivatesNextHighestContainer) {
  wm::ActivateWindow(w7());
  EXPECT_TRUE(wm::IsActiveWindow(w7()));

  w7()->Hide();
  EXPECT_TRUE(wm::IsActiveWindow(w6()));
}

// Test that hiding a window in a higher container with no other windows will
// activate a window in a lower container.
TEST_F(GetTopmostWindowToActivateTest, HideActivatesNextHighestContainer) {
  w5()->Hide();
  wm::ActivateWindow(w6());
  EXPECT_TRUE(wm::IsActiveWindow(w6()));

  w6()->Hide();
  EXPECT_TRUE(wm::IsActiveWindow(w4()));
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
  EXPECT_EQ(NULL, wm::GetActiveWindow());

  // Clicking on an activatable window activtes the window.
  aura::test::EventGenerator generator(Shell::GetRootWindow(), w1.get());
  generator.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Creates a menu that covers the transient parent.
  scoped_ptr<aura::Window> menu(aura::test::CreateTestWindowWithDelegateAndType(
      &wd, aura::client::WINDOW_TYPE_MENU, 2, gfx::Rect(100, 100), NULL));
  ad2.SetWindow(menu.get());
  w1->AddTransientChild(menu.get());

  // Clicking on a menu whose transient parent is active window shouldn't
  // change the active window.
  generator.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
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
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w2.get(), parent->children()[1]);

  // Activate w1 and make sure it's active and frontmost.
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_EQ(w1.get(), parent->children()[1]);

  // Deactivate w1 and make sure w2 becomes active and frontmost.
  wm::DeactivateWindow(w1.get());
  EXPECT_FALSE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
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
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(1, ad1.activated_count());
  // Should not have gotten a OnLostActive yet.
  EXPECT_EQ(0, ad1.lost_active_count());

  // Deactivate the active window.
  wm::DeactivateWindow(w1.get());
  EXPECT_FALSE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(1, ad1.lost_active_count());
  EXPECT_FALSE(ad1.window_was_active());

  // Activate w1 again. w1 should have gotten OnActivated.
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(2, ad1.activated_count());
  EXPECT_EQ(1, ad1.lost_active_count());

  // Reset the delegate.
  ad1.Clear();

  // Now activate another window.
  wm::ActivateWindow(w2.get());

  // Should have gotten OnLostActive and w1 shoouldn't have been
  // active window in OnLostActive.
  EXPECT_EQ(0, ad1.activated_count());
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

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  w2->Focus();
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_TRUE(w2->HasFocus());

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  w21->Focus();
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_TRUE(w21->HasFocus());
}

// Verifies that we prevent all attempts to focus a child of a non-activatable
// window from claiming focus to that window.
TEST_F(ActivationControllerTest, PreventFocusToNonActivatableWindow) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &wd, -1, gfx::Rect(50, 50), NULL));
  // The RootWindow is a non-activatable parent.
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &wd, -2, gfx::Rect(50, 50), Shell::GetRootWindow()));
  scoped_ptr<aura::Window> w21(aura::test::CreateTestWindowWithDelegate(
      &wd, -21, gfx::Rect(50, 50), w2.get()));

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  // Try activating |w2|. It's not a child of an activatable container, so it
  // should neither be activated nor get focus.
  wm::ActivateWindow(w2.get());
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(w2->HasFocus());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  // Try focusing |w2|. Same rules apply.
  w2->Focus();
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(w2->HasFocus());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());

  // Try focusing |w21|. Same rules apply.
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(w21->HasFocus());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->HasFocus());
}

TEST_F(ActivationControllerTest, CanActivateWindowIteselfTest)
{
  aura::test::TestWindowDelegate wd;

  // Normal Window
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &wd, -1, gfx::Rect(50, 50), NULL));
  EXPECT_TRUE(wm::CanActivateWindow(w1.get()));

  // The RootWindow is a non-activatable parent.
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &wd, -2, gfx::Rect(50, 50), Shell::GetRootWindow()));
  scoped_ptr<aura::Window> w21(aura::test::CreateTestWindowWithDelegate(
      &wd, -21, gfx::Rect(50, 50), w2.get()));
  EXPECT_FALSE(wm::CanActivateWindow(w2.get()));
  EXPECT_FALSE(wm::CanActivateWindow(w21.get()));

  // The window has a transient child.
  scoped_ptr<aura::Window> w3(aura::test::CreateTestWindowWithDelegate(
      &wd, -3, gfx::Rect(50, 50), NULL));
  scoped_ptr<aura::Window> w31(aura::test::CreateTestWindowWithDelegate(
      &wd, -31, gfx::Rect(50, 50), NULL));
  w3->AddTransientChild(w31.get());
  EXPECT_TRUE(wm::CanActivateWindow(w3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(w31.get()));

  // The window has a transient window-modal child.
  scoped_ptr<aura::Window> w4(aura::test::CreateTestWindowWithDelegate(
      &wd, -4, gfx::Rect(50, 50), NULL));
  scoped_ptr<aura::Window> w41(aura::test::CreateTestWindowWithDelegate(
      &wd, -41, gfx::Rect(50, 50), NULL));
  w4->AddTransientChild(w41.get());
  w41->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  EXPECT_FALSE(wm::CanActivateWindow(w4.get()));
  EXPECT_TRUE(wm::CanActivateWindow(w41.get()));

  // The window has a transient system-modal child.
  scoped_ptr<aura::Window> w5(aura::test::CreateTestWindowWithDelegate(
      &wd, -5, gfx::Rect(50, 50), NULL));
  scoped_ptr<aura::Window> w51(aura::test::CreateTestWindowWithDelegate(
      &wd, -51, gfx::Rect(50, 50), NULL));
  w5->AddTransientChild(w51.get());
  w51->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
  EXPECT_TRUE(wm::CanActivateWindow(w5.get()));
  EXPECT_TRUE(wm::CanActivateWindow(w51.get()));
}

// Verifies code in ActivationController::OnWindowVisibilityChanged() that keeps
// hiding windows layers stacked above the newly active window while they
// animate away.
TEST_F(ActivationControllerTest, AnimateHideMaintainsStacking) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
    &wd, -1, gfx::Rect(50, 50, 50, 50), NULL));
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &wd, -2, gfx::Rect(75, 75, 50, 50), NULL));
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  w2->Hide();
  typedef std::vector<ui::Layer*> Layers;
  const Layers& children = w1->parent()->layer()->children();
  Layers::const_iterator w1_iter =
      std::find(children.begin(), children.end(), w1->layer());
  Layers::const_iterator w2_iter =
      std::find(children.begin(), children.end(), w2->layer());
  EXPECT_TRUE(w2_iter > w1_iter);
}

// Verifies that activating a minimized window would restore it.
TEST_F(ActivationControllerTest, ActivateMinimizedWindow) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &wd, -1, gfx::Rect(50, 50), NULL));

  wm::MinimizeWindow(w1.get());
  EXPECT_TRUE(wm::IsWindowMinimized(w1.get()));

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_FALSE(wm::IsWindowMinimized(w1.get()));
}

// Verifies that a minimized window would not be automatically activated as
// a replacement active window.
TEST_F(ActivationControllerTest, NoAutoActivateMinimizedWindow) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
    &wd, -1, gfx::Rect(50, 50, 50, 50), NULL));
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &wd, -2, gfx::Rect(75, 75, 50, 50), NULL));

  wm::MinimizeWindow(w1.get());
  EXPECT_TRUE(wm::IsWindowMinimized(w1.get()));

  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  w2->Hide();

  EXPECT_FALSE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(wm::IsWindowMinimized(w1.get()));
}

// Verifies that a window with a hidden layer can be activated.
TEST_F(ActivationControllerTest, ActivateWithHiddenLayer) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
    &wd, -1, gfx::Rect(50, 50, 50, 50), NULL));

  EXPECT_TRUE(wm::CanActivateWindow(w1.get()));
  w1->layer()->SetVisible(false);
  EXPECT_TRUE(wm::CanActivateWindow(w1.get()));
}

}  // namespace test
}  // namespace ash
