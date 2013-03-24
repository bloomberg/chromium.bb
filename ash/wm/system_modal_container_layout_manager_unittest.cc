// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_modal_container_layout_manager.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/views/test/capture_tracking_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {

namespace {

aura::Window* GetModalContainer() {
  return Shell::GetPrimaryRootWindowController()->GetContainer(
      ash::internal::kShellWindowId_SystemModalContainer);
}

bool AllRootWindowsHaveModalBackgroundsForContainer(int container_id) {
  std::vector<aura::Window*> containers =
      Shell::GetContainersFromAllRootWindows(container_id, NULL);
  bool has_modal_screen = !containers.empty();
  for (std::vector<aura::Window*>::iterator iter = containers.begin();
       iter != containers.end(); ++iter) {
    has_modal_screen &=
        static_cast<internal::SystemModalContainerLayoutManager*>(
            (*iter)->layout_manager())->has_modal_background();
  }
  return has_modal_screen;
}

bool AllRootWindowsHaveLockedModalBackgrounds() {
  return AllRootWindowsHaveModalBackgroundsForContainer(
      internal::kShellWindowId_LockSystemModalContainer);
}

bool AllRootWindowsHaveModalBackgrounds() {
  return AllRootWindowsHaveModalBackgroundsForContainer(
      internal::kShellWindowId_SystemModalContainer);
}

class TestWindow : public views::WidgetDelegateView {
 public:
  explicit TestWindow(bool modal) : modal_(modal) {}
  virtual ~TestWindow() {}

  // The window needs be closed from widget in order for
  // aura::client::kModalKey property to be reset.
  static void CloseTestWindow(aura::Window* window) {
    views::Widget::GetWidgetForNativeWindow(window)->Close();
  }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(50, 50);
  }

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return modal_ ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_NONE;
  }

 private:
  bool modal_;

  DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

class EventTestWindow : public TestWindow {
 public:
  explicit EventTestWindow(bool modal) : TestWindow(modal),
                                         mouse_presses_(0) {}
  virtual ~EventTestWindow() {}

  aura::Window* OpenTestWindowWithContext(aura::RootWindow* context) {
    views::Widget* widget =
        views::Widget::CreateWindowWithContext(this, context);
    widget->Show();
    return widget->GetNativeView();
  }

  aura::Window* OpenTestWindowWithParent(aura::Window* parent) {
    DCHECK(parent);
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(this, parent);
    widget->Show();
    return widget->GetNativeView();
  }

  // Overridden from views::View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    mouse_presses_++;
    return false;
  }

  int mouse_presses() const { return mouse_presses_; }
 private:
  int mouse_presses_;

  DISALLOW_COPY_AND_ASSIGN(EventTestWindow);
};

class TransientWindowObserver : public aura::WindowObserver {
 public:
  TransientWindowObserver() : destroyed_(false) {}
  virtual ~TransientWindowObserver() {}

  bool destroyed() const { return destroyed_; }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    destroyed_ = true;
  }

 private:
  bool destroyed_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowObserver);
};

}  // namespace

class SystemModalContainerLayoutManagerTest : public AshTestBase {
 public:
  aura::Window* OpenToplevelTestWindow(bool modal) {
    views::Widget* widget = views::Widget::CreateWindowWithContext(
        new TestWindow(modal), CurrentContext());
    widget->Show();
    return widget->GetNativeView();
  }

  aura::Window* OpenTestWindowWithParent(aura::Window* parent, bool modal) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new TestWindow(modal), parent);
    widget->Show();
    return widget->GetNativeView();
  }
};

TEST_F(SystemModalContainerLayoutManagerTest, NonModalTransient) {
  scoped_ptr<aura::Window> parent(OpenToplevelTestWindow(false));
  aura::Window* transient = OpenTestWindowWithParent(parent.get(), false);
  TransientWindowObserver destruction_observer;
  transient->AddObserver(&destruction_observer);

  EXPECT_EQ(parent.get(), transient->transient_parent());
  EXPECT_EQ(parent->parent(), transient->parent());

  // The transient should be destroyed with its parent.
  parent.reset();
  EXPECT_TRUE(destruction_observer.destroyed());
}

TEST_F(SystemModalContainerLayoutManagerTest, ModalTransient) {
  scoped_ptr<aura::Window> parent(OpenToplevelTestWindow(false));
  // parent should be active.
  EXPECT_TRUE(wm::IsActiveWindow(parent.get()));
  aura::Window* t1 = OpenTestWindowWithParent(parent.get(), true);

  TransientWindowObserver do1;
  t1->AddObserver(&do1);

  EXPECT_EQ(parent.get(), t1->transient_parent());
  EXPECT_EQ(GetModalContainer(), t1->parent());

  // t1 should now be active.
  EXPECT_TRUE(wm::IsActiveWindow(t1));

  // Attempting to click the parent should result in no activation change.
  aura::test::EventGenerator e1(Shell::GetPrimaryRootWindow(), parent.get());
  e1.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(t1));

  // Now open another modal transient parented to the original modal transient.
  aura::Window* t2 = OpenTestWindowWithParent(t1, true);
  TransientWindowObserver do2;
  t2->AddObserver(&do2);

  EXPECT_TRUE(wm::IsActiveWindow(t2));

  EXPECT_EQ(t1, t2->transient_parent());
  EXPECT_EQ(GetModalContainer(), t2->parent());

  // t2 should still be active, even after clicking on t1.
  aura::test::EventGenerator e2(Shell::GetPrimaryRootWindow(), t1);
  e2.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(t2));

  // Both transients should be destroyed with parent.
  parent.reset();
  EXPECT_TRUE(do1.destroyed());
  EXPECT_TRUE(do2.destroyed());
}

TEST_F(SystemModalContainerLayoutManagerTest, ModalNonTransient) {
  scoped_ptr<aura::Window> t1(OpenToplevelTestWindow(true));
  // parent should be active.
  EXPECT_TRUE(wm::IsActiveWindow(t1.get()));
  TransientWindowObserver do1;
  t1->AddObserver(&do1);

  EXPECT_EQ(NULL, t1->transient_parent());
  EXPECT_EQ(GetModalContainer(), t1->parent());

  // t1 should now be active.
  EXPECT_TRUE(wm::IsActiveWindow(t1.get()));

  // Attempting to click the parent should result in no activation change.
  aura::test::EventGenerator e1(Shell::GetPrimaryRootWindow(),
                                Shell::GetPrimaryRootWindow());
  e1.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(t1.get()));

  // Now open another modal transient parented to the original modal transient.
  aura::Window* t2 = OpenTestWindowWithParent(t1.get(), true);
  TransientWindowObserver do2;
  t2->AddObserver(&do2);

  EXPECT_TRUE(wm::IsActiveWindow(t2));

  EXPECT_EQ(t1, t2->transient_parent());
  EXPECT_EQ(GetModalContainer(), t2->parent());

  // t2 should still be active, even after clicking on t1.
  aura::test::EventGenerator e2(Shell::GetPrimaryRootWindow(), t1.get());
  e2.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(t2));

  // Both transients should be destroyed with parent.
  t1.reset();
  EXPECT_TRUE(do1.destroyed());
  EXPECT_TRUE(do2.destroyed());
}

// Fails on Mac only.  Needs to be implemented.  http://crbug.com/111279.
#if defined(OS_MACOSX)
#define MAYBE_CanActivateAfterEndModalSession \
    DISABLED_CanActivateAfterEndModalSession
#else
#define MAYBE_CanActivateAfterEndModalSession CanActivateAfterEndModalSession
#endif
// Tests that we can activate an unrelated window after a modal window is closed
// for a window.
TEST_F(SystemModalContainerLayoutManagerTest,
       MAYBE_CanActivateAfterEndModalSession) {
  scoped_ptr<aura::Window> unrelated(OpenToplevelTestWindow(false));
  unrelated->SetBounds(gfx::Rect(100, 100, 50, 50));
  scoped_ptr<aura::Window> parent(OpenToplevelTestWindow(false));
  // parent should be active.
  EXPECT_TRUE(wm::IsActiveWindow(parent.get()));

  scoped_ptr<aura::Window> transient(
      OpenTestWindowWithParent(parent.get(), true));
  // t1 should now be active.
  EXPECT_TRUE(wm::IsActiveWindow(transient.get()));

  // Attempting to click the parent should result in no activation change.
  aura::test::EventGenerator e1(Shell::GetPrimaryRootWindow(), parent.get());
  e1.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(transient.get()));

  // Now close the transient.
  transient->Hide();
  TestWindow::CloseTestWindow(transient.release());

  base::RunLoop().RunUntilIdle();

  // parent should now be active again.
  EXPECT_TRUE(wm::IsActiveWindow(parent.get()));

  // Attempting to click unrelated should activate it.
  aura::test::EventGenerator e2(Shell::GetPrimaryRootWindow(), unrelated.get());
  e2.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(unrelated.get()));
}

TEST_F(SystemModalContainerLayoutManagerTest, EventFocusContainers) {
  // Create a normal window and attempt to receive a click event.
  EventTestWindow* main_delegate = new EventTestWindow(false);
  scoped_ptr<aura::Window> main(
      main_delegate->OpenTestWindowWithContext(CurrentContext()));
  EXPECT_TRUE(wm::IsActiveWindow(main.get()));
  aura::test::EventGenerator e1(Shell::GetPrimaryRootWindow(), main.get());
  e1.ClickLeftButton();
  EXPECT_EQ(1, main_delegate->mouse_presses());

  // Create a modal window for the main window and verify that the main window
  // no longer receives mouse events.
  EventTestWindow* transient_delegate = new EventTestWindow(true);
  aura::Window* transient =
      transient_delegate->OpenTestWindowWithParent(main.get());
  EXPECT_TRUE(wm::IsActiveWindow(transient));
  e1.ClickLeftButton();
  EXPECT_EQ(1, transient_delegate->mouse_presses());

  // Create a window in the lock screen container and ensure that it receives
  // the mouse event instead of the modal window (crbug.com/110920).
  Shell::GetInstance()->delegate()->LockScreen();
  EventTestWindow* lock_delegate = new EventTestWindow(false);
  scoped_ptr<aura::Window> lock(lock_delegate->OpenTestWindowWithParent(
      Shell::GetPrimaryRootWindowController()->GetContainer(
          ash::internal::kShellWindowId_LockScreenContainer)));
  EXPECT_TRUE(wm::IsActiveWindow(lock.get()));
  e1.ClickLeftButton();
  EXPECT_EQ(1, lock_delegate->mouse_presses());

  // Make sure that a modal container created by the lock screen can still
  // receive mouse events.
  EventTestWindow* lock_modal_delegate = new EventTestWindow(true);
  aura::Window* lock_modal =
      lock_modal_delegate->OpenTestWindowWithParent(lock.get());
  EXPECT_TRUE(wm::IsActiveWindow(lock_modal));
  e1.ClickLeftButton();
  EXPECT_EQ(1, lock_modal_delegate->mouse_presses());

  // Verify that none of the other containers received any more mouse presses.
  EXPECT_EQ(1, main_delegate->mouse_presses());
  EXPECT_EQ(1, transient_delegate->mouse_presses());
  EXPECT_EQ(1, lock_delegate->mouse_presses());
  EXPECT_EQ(1, lock_modal_delegate->mouse_presses());

  Shell::GetInstance()->delegate()->UnlockScreen();
}

// Makes sure we don't crash if a modal window is shown while the parent window
// is hidden.
TEST_F(SystemModalContainerLayoutManagerTest, ShowModalWhileHidden) {
  // Hide the lock screen.
  Shell::GetPrimaryRootWindowController()->GetContainer(
      internal::kShellWindowId_SystemModalContainer)->layer()->SetOpacity(0);

  // Create a modal window.
  scoped_ptr<aura::Window> parent(OpenToplevelTestWindow(false));
  scoped_ptr<aura::Window> modal_window(
      OpenTestWindowWithParent(parent.get(), true));
  parent->Show();
  modal_window->Show();
}

// Verifies we generate a capture lost when showing a modal window.
TEST_F(SystemModalContainerLayoutManagerTest, ChangeCapture) {
  views::Widget* widget = views::Widget::CreateWindowWithContext(
      new TestWindow(false), CurrentContext());
  scoped_ptr<aura::Window> widget_window(widget->GetNativeView());
  views::test::CaptureTrackingView* view = new views::test::CaptureTrackingView;
  widget->GetContentsView()->AddChildView(view);
  view->SetBoundsRect(widget->GetContentsView()->bounds());
  widget->Show();

  gfx::Point center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &center);
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), center);
  generator.PressLeftButton();
  EXPECT_TRUE(view->got_press());
  scoped_ptr<aura::Window> modal_window(
      OpenTestWindowWithParent(widget->GetNativeView(), true));
  modal_window->Show();
  EXPECT_TRUE(view->got_capture_lost());
}

// Verifies that the window gets moved into the visible screen area upon screen
// resize.
TEST_F(SystemModalContainerLayoutManagerTest, KeepVisible) {
  GetModalContainer()->SetBounds(gfx::Rect(0, 0, 1024, 768));
  scoped_ptr<aura::Window> main(OpenTestWindowWithParent(GetModalContainer(),
                                                         true));
  main->SetBounds(gfx::Rect(924, 668, 100, 100));
  // We set now the bounds of the root window to something new which will
  // Then trigger the repos operation.
  GetModalContainer()->SetBounds(gfx::Rect(0, 0, 800, 600));

  gfx::Rect bounds = main->bounds();
  EXPECT_EQ(bounds, gfx::Rect(700, 500, 100, 100));
}

TEST_F(SystemModalContainerLayoutManagerTest, ShowNormalBackgroundOrLocked) {
  scoped_ptr<aura::Window> parent(OpenToplevelTestWindow(false));
  scoped_ptr<aura::Window> modal_window(
      OpenTestWindowWithParent(parent.get(), true));
  parent->Show();
  modal_window->Show();

  // Normal system modal window.  Shows normal system modal background and not
  // locked.
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_FALSE(AllRootWindowsHaveLockedModalBackgrounds());

  TestWindow::CloseTestWindow(modal_window.release());
  EXPECT_FALSE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_FALSE(AllRootWindowsHaveLockedModalBackgrounds());

  // Normal system modal window while locked.  Shows locked system modal
  // background.
  Shell::GetInstance()->delegate()->LockScreen();
  scoped_ptr<aura::Window> lock_parent(OpenTestWindowWithParent(
      Shell::GetPrimaryRootWindowController()->GetContainer(
          ash::internal::kShellWindowId_LockScreenContainer),
      false));
  scoped_ptr<aura::Window> lock_modal_window(OpenTestWindowWithParent(
      lock_parent.get(), true));
  lock_parent->Show();
  lock_modal_window->Show();
  EXPECT_FALSE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(AllRootWindowsHaveLockedModalBackgrounds());
  TestWindow::CloseTestWindow(lock_modal_window.release());

  // Normal system modal window while locked, but it belongs to the normal
  // window.  Shouldn't show locked system modal background, but normal.
  scoped_ptr<aura::Window> modal_window2(
      OpenTestWindowWithParent(parent.get(), true));
  modal_window2->Show();
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_FALSE(AllRootWindowsHaveLockedModalBackgrounds());
  TestWindow::CloseTestWindow(modal_window2.release());

  // Here we should check the behavior of the locked system modal dialog when
  // unlocked, but such case isn't handled very well right now.
  // See crbug.com/157660
  // TODO(mukai): add the test case when the bug is fixed.
}

#if defined(OS_WIN)
// Multiple displays are not supported on Windows Ash. http://crbug.com/165962
#define MAYBE_MultiDisplays DISABLED_MultiDisplays
#else
#define MAYBE_MultiDisplays MultiDisplays
#endif

TEST_F(SystemModalContainerLayoutManagerTest, MAYBE_MultiDisplays) {
  UpdateDisplay("500x500,500x500");

  scoped_ptr<aura::Window> normal(OpenToplevelTestWindow(false));
  normal->SetBounds(gfx::Rect(100, 100, 50, 50));

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());
  aura::Window* container1 = Shell::GetContainer(
      root_windows[0], ash::internal::kShellWindowId_SystemModalContainer);
  aura::Window* container2 = Shell::GetContainer(
      root_windows[1], ash::internal::kShellWindowId_SystemModalContainer);

  scoped_ptr<aura::Window> modal1(
      OpenTestWindowWithParent(container1, true));
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal1.get()));

  scoped_ptr<aura::Window> modal11(
      OpenTestWindowWithParent(container1, true));
  EXPECT_TRUE(wm::IsActiveWindow(modal11.get()));

  scoped_ptr<aura::Window> modal2(
      OpenTestWindowWithParent(container2, true));
  EXPECT_TRUE(wm::IsActiveWindow(modal2.get()));

  // Sanity check if they're on the correct containers.
  EXPECT_EQ(container1, modal1->parent());
  EXPECT_EQ(container1, modal11->parent());
  EXPECT_EQ(container2, modal2->parent());

  TestWindow::CloseTestWindow(modal2.release());
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal11.get()));

  TestWindow::CloseTestWindow(modal11.release());
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal1.get()));

  UpdateDisplay("500x500");
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal1.get()));

  UpdateDisplay("500x500,600x600");
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal1.get()));

  // No more modal screen.
  modal1->Hide();
  TestWindow::CloseTestWindow(modal1.release());
  EXPECT_FALSE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(normal.get()));
}

}  // namespace test
}  // namespace ash
