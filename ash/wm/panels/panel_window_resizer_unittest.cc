// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_window_resizer.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/i18n/rtl.h"
#include "base/win/windows_version.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class PanelWindowResizerTest : public test::AshTestBase {
 public:
  PanelWindowResizerTest() {}
  ~PanelWindowResizerTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("600x400");
    test::ShellTestApi test_api(Shell::GetInstance());
    model_ = test_api.shelf_model();
    shelf_delegate_ = test::TestShelfDelegate::instance();
  }

  void TearDown() override { AshTestBase::TearDown(); }

 protected:
  gfx::Point CalculateDragPoint(const WindowResizer& resizer,
                                int delta_x,
                                int delta_y) const {
    gfx::Point location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  aura::Window* CreatePanelWindow(const gfx::Point& origin) {
    gfx::Rect bounds(origin, gfx::Size(101, 101));
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL, ui::wm::WINDOW_TYPE_PANEL, 0, bounds);
    shelf_delegate_->AddShelfItem(window);
    return window;
  }

  void DragStart(aura::Window* window) {
    resizer_.reset(CreateWindowResizer(
        window,
        window->bounds().origin(),
        HTCAPTION,
        aura::client::WINDOW_MOVE_SOURCE_MOUSE).release());
    ASSERT_TRUE(resizer_.get());
  }

  void DragMove(int dx, int dy) {
    resizer_->Drag(CalculateDragPoint(*resizer_, dx, dy), 0);
  }

  void DragEnd() {
    resizer_->CompleteDrag();
    resizer_.reset();
  }

  void DragRevert() {
    resizer_->RevertDrag();
    resizer_.reset();
  }

  // Test dragging the panel slightly, then detaching, and then reattaching
  // dragging out by the vector (dx, dy).
  void DetachReattachTest(aura::Window* window, int dx, int dy) {
    wm::WindowState* window_state = wm::GetWindowState(window);
    EXPECT_TRUE(window_state->panel_attached());
    aura::Window* root_window = window->GetRootWindow();
    EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
    DragStart(window);
    gfx::Rect initial_bounds = window->GetBoundsInScreen();

    // Drag the panel slightly. The window should still be snapped to the
    // launcher.
    DragMove(dx * 5, dy * 5);
    EXPECT_EQ(initial_bounds.x(), window->GetBoundsInScreen().x());
    EXPECT_EQ(initial_bounds.y(), window->GetBoundsInScreen().y());

    // Drag further out and the window should now move to the cursor.
    DragMove(dx * 100, dy * 100);
    EXPECT_EQ(initial_bounds.x() + dx * 100, window->GetBoundsInScreen().x());
    EXPECT_EQ(initial_bounds.y() + dy * 100, window->GetBoundsInScreen().y());

    // The panel should be detached when the drag completes.
    DragEnd();

    EXPECT_FALSE(window_state->panel_attached());
    EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
    EXPECT_EQ(root_window, window->GetRootWindow());

    DragStart(window);
    // Drag the panel down.
    DragMove(dx * -95, dy * -95);
    // Release the mouse and the panel should be reattached.
    DragEnd();

    // The panel should be reattached and have snapped to the launcher.
    EXPECT_TRUE(window_state->panel_attached());
    EXPECT_EQ(initial_bounds.x(), window->GetBoundsInScreen().x());
    EXPECT_EQ(initial_bounds.y(), window->GetBoundsInScreen().y());
    EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
  }

  void TestWindowOrder(const std::vector<aura::Window*>& window_order) {
    int panel_index = model_->FirstPanelIndex();
    EXPECT_EQ((int)(panel_index + window_order.size()), model_->item_count());
    for (std::vector<aura::Window*>::const_iterator iter =
         window_order.begin(); iter != window_order.end();
         ++iter, ++panel_index) {
      ShelfID id = GetShelfIDForWindow(*iter);
      EXPECT_EQ(id, model_->items()[panel_index].id);
    }
  }

  // Test dragging panel window along the shelf and verify that panel icons
  // are reordered appropriately.
  void DragAlongShelfReorder(int dx, int dy) {
    gfx::Point origin(0, 0);
    scoped_ptr<aura::Window> w1(CreatePanelWindow(origin));
    scoped_ptr<aura::Window> w2(CreatePanelWindow(origin));
    std::vector<aura::Window*> window_order_original;
    std::vector<aura::Window*> window_order_swapped;
    window_order_original.push_back(w1.get());
    window_order_original.push_back(w2.get());
    window_order_swapped.push_back(w2.get());
    window_order_swapped.push_back(w1.get());
    TestWindowOrder(window_order_original);

    // Drag window #2 to the beginning of the shelf.
    DragStart(w2.get());
    DragMove(400 * dx, 400 * dy);
    TestWindowOrder(window_order_swapped);
    DragEnd();

    // Expect swapped window order.
    TestWindowOrder(window_order_swapped);

    // Drag window #2 back to the end.
    DragStart(w2.get());
    DragMove(-400 * dx, -400 * dy);
    TestWindowOrder(window_order_original);
    DragEnd();

    // Expect original order.
    TestWindowOrder(window_order_original);
  }

 private:
  scoped_ptr<WindowResizer> resizer_;
  ShelfModel* model_;
  test::TestShelfDelegate* shelf_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PanelWindowResizerTest);
};

class PanelWindowResizerTextDirectionTest
    : public PanelWindowResizerTest,
      public testing::WithParamInterface<bool> {
 public:
  PanelWindowResizerTextDirectionTest() : is_rtl_(GetParam()) {}
  virtual ~PanelWindowResizerTextDirectionTest() {}

  void SetUp() override {
    original_locale_ = base::i18n::GetConfiguredLocale();
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale("he");
    PanelWindowResizerTest::SetUp();
    ASSERT_EQ(is_rtl_, base::i18n::IsRTL());
  }

  void TearDown() override {
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale(original_locale_);
    PanelWindowResizerTest::TearDown();
  }

 private:
  bool is_rtl_;
  std::string original_locale_;

  DISALLOW_COPY_AND_ASSIGN(PanelWindowResizerTextDirectionTest);
};

// PanelLayoutManager and PanelWindowResizer should work if panels have
// transient children of supported types.
class PanelWindowResizerTransientTest
    : public PanelWindowResizerTest,
      public testing::WithParamInterface<ui::wm::WindowType> {
 public:
  PanelWindowResizerTransientTest() : transient_window_type_(GetParam()) {}
  virtual ~PanelWindowResizerTransientTest() {}

 protected:
  ui::wm::WindowType transient_window_type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PanelWindowResizerTransientTest);
};

// Verifies a window can be dragged from the panel and detached and then
// reattached.
TEST_F(PanelWindowResizerTest, PanelDetachReattachBottom) {
 if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  DetachReattachTest(window.get(), 0, -1);
}

TEST_F(PanelWindowResizerTest, PanelDetachReattachLeft) {
 if (!SupportsHostWindowResize())
    return;

  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_LEFT, shell->GetPrimaryRootWindow());
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  DetachReattachTest(window.get(), 1, 0);
}

TEST_F(PanelWindowResizerTest, PanelDetachReattachRight) {
  if (!SupportsHostWindowResize())
    return;

  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_RIGHT,
                           shell->GetPrimaryRootWindow());
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  DetachReattachTest(window.get(), -1, 0);
}

TEST_F(PanelWindowResizerTest, PanelDetachReattachTop) {
 if (!SupportsHostWindowResize())
    return;

  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_TOP, shell->GetPrimaryRootWindow());
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  DetachReattachTest(window.get(), 0, 1);
}

// Tests that a drag continues when the shelf is hidden. This occurs as part of
// the animation when switching profiles. http://crbug.com/393047.
TEST_F(PanelWindowResizerTest, DetachThenHideShelf) {
  if (!SupportsHostWindowResize())
    return;
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  wm::WindowState* state = wm::GetWindowState(window.get());
  gfx::Rect expected_bounds = window->GetBoundsInScreen();
  expected_bounds.set_y(expected_bounds.y() - 100);
  DragStart(window.get());
  DragMove(0, -100);
  EXPECT_FALSE(state->IsMinimized());

  // Hide the shelf. This minimizes all attached windows but should ignore
  // the dragged window.
  ShelfLayoutManager* shelf = RootWindowController::ForWindow(window.get())->
      shelf()->shelf_layout_manager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  shelf->UpdateVisibilityState();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(state->IsMinimized());
  EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
  DragEnd();

  // When the drag ends the window should be detached and placed where it was
  // dragged to.
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  EXPECT_FALSE(state->IsMinimized());
  EXPECT_EQ(expected_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(PanelWindowResizerTest, PanelDetachReattachMultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(600, 0)));
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  DetachReattachTest(window.get(), 0, -1);
}

TEST_F(PanelWindowResizerTest, DetachThenDragAcrossDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  gfx::Rect initial_bounds = window->GetBoundsInScreen();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  DragStart(window.get());
  DragMove(0, -100);
  DragEnd();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ(initial_bounds.x(), window->GetBoundsInScreen().x());
  EXPECT_EQ(initial_bounds.y() - 100, window->GetBoundsInScreen().y());
  EXPECT_FALSE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());

  DragStart(window.get());
  DragMove(500, 0);
  DragEnd();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(initial_bounds.x() + 500, window->GetBoundsInScreen().x());
  EXPECT_EQ(initial_bounds.y() - 100, window->GetBoundsInScreen().y());
  EXPECT_FALSE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

TEST_F(PanelWindowResizerTest, DetachAcrossDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  gfx::Rect initial_bounds = window->GetBoundsInScreen();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  DragStart(window.get());
  DragMove(500, -100);
  DragEnd();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(initial_bounds.x() + 500, window->GetBoundsInScreen().x());
  EXPECT_EQ(initial_bounds.y() - 100, window->GetBoundsInScreen().y());
  EXPECT_FALSE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

TEST_F(PanelWindowResizerTest, DetachThenAttachToSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,600x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  gfx::Rect initial_bounds = window->GetBoundsInScreen();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  // Detach the window.
  DragStart(window.get());
  DragMove(0, -100);
  DragEnd();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_FALSE(wm::GetWindowState(window.get())->panel_attached());

  // Drag the window just above the other display's launcher.
  DragStart(window.get());
  DragMove(500, 295);
  EXPECT_EQ(initial_bounds.x() + 500, window->GetBoundsInScreen().x());

  // Should stick to other launcher.
  EXPECT_EQ(initial_bounds.y() + 200, window->GetBoundsInScreen().y());
  DragEnd();

  // When dropped should move to second display's panel container.
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_TRUE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
}

TEST_F(PanelWindowResizerTest, AttachToSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,600x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  gfx::Rect initial_bounds = window->GetBoundsInScreen();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  // Drag the window just above the other display's launcher.
  DragStart(window.get());
  DragMove(500, 195);
  EXPECT_EQ(initial_bounds.x() + 500, window->GetBoundsInScreen().x());

  // Should stick to other launcher.
  EXPECT_EQ(initial_bounds.y() + 200, window->GetBoundsInScreen().y());
  DragEnd();

  // When dropped should move to second display's panel container.
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_TRUE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
}

TEST_F(PanelWindowResizerTest, AttachToSecondFullscreenDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,600x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  scoped_ptr<aura::Window> fullscreen(
      CreateTestWindowInShellWithBounds(gfx::Rect(600, 0, 101, 101)));
  wm::GetWindowState(fullscreen.get())->Activate();
  const wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(fullscreen.get())->OnWMEvent(&event);
  EXPECT_TRUE(wm::GetWindowState(fullscreen.get())->IsFullscreen());

  gfx::Rect initial_bounds = window->GetBoundsInScreen();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  // Activate and drag the window to the other display's launcher.
  wm::GetWindowState(window.get())->Activate();
  DragStart(window.get());
  DragMove(500, 250);
  EXPECT_EQ(initial_bounds.x() + 500, window->GetBoundsInScreen().x());
  EXPECT_GT(window->GetBoundsInScreen().y(),
            initial_bounds.y() + 200);
  DragEnd();

  // When dropped should move to second display's panel container.
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_TRUE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsActive());
  EXPECT_EQ(initial_bounds.y() + 200, window->GetBoundsInScreen().y());
}

TEST_F(PanelWindowResizerTest, RevertDragRestoresAttachment) {
  scoped_ptr<aura::Window> window(
      CreatePanelWindow(gfx::Point(0, 0)));
  EXPECT_TRUE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
  DragStart(window.get());
  DragMove(0, -100);
  DragRevert();
  EXPECT_TRUE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());

  // Detach panel.
  DragStart(window.get());
  DragMove(0, -100);
  DragEnd();
  EXPECT_FALSE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());

  // Drag back to launcher.
  DragStart(window.get());
  DragMove(0, 100);

  // When the drag is reverted it should remain detached.
  DragRevert();
  EXPECT_FALSE(wm::GetWindowState(window.get())->panel_attached());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

TEST_F(PanelWindowResizerTest, DragMovesToPanelLayer) {
  scoped_ptr<aura::Window> window(CreatePanelWindow(gfx::Point(0, 0)));
  DragStart(window.get());
  DragMove(0, -100);
  DragEnd();
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());

  // While moving the panel window should be moved to the panel container.
  DragStart(window.get());
  DragMove(20, 0);
  EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
  DragEnd();

  // When dropped it should return to the default container.
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

TEST_P(PanelWindowResizerTextDirectionTest, DragReordersPanelsHorizontal) {
  if (!SupportsHostWindowResize())
    return;

  DragAlongShelfReorder(base::i18n::IsRTL() ? 1 : -1, 0);
}

TEST_F(PanelWindowResizerTest, DragReordersPanelsVertical) {
  if (!SupportsHostWindowResize())
    return;

  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_LEFT, shell->GetPrimaryRootWindow());
  DragAlongShelfReorder(0, -1);
}

// Tests that panels can have transient children of different types.
// The transient children should be reparented in sync with the panel.
TEST_P(PanelWindowResizerTransientTest, PanelWithTransientChild) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreatePanelWindow(gfx::Point(0, 0)));
  scoped_ptr<aura::Window> child(CreateTestWindowInShellWithDelegateAndType(
      NULL, transient_window_type_, 0, gfx::Rect(20, 20, 150, 40)));
  ::wm::AddTransientChild(window.get(), child.get());
  if (window->parent() != child->parent())
    window->parent()->AddChild(child.get());
  EXPECT_EQ(window.get(), ::wm::GetTransientParent(child.get()));

  // Drag the child to the shelf. Its new position should not be overridden.
  const gfx::Rect attached_bounds(window->GetBoundsInScreen());
  const int dy = window->GetBoundsInScreen().bottom() -
      child->GetBoundsInScreen().bottom();
  DragStart(child.get());
  DragMove(50, dy);
  // While moving the transient child window should be in the panel container.
  EXPECT_EQ(kShellWindowId_PanelContainer, child->parent()->id());
  DragEnd();
  // Child should move, |window| should not.
  EXPECT_EQ(gfx::Point(20 + 50, 20 + dy).ToString(),
            child->GetBoundsInScreen().origin().ToString());
  EXPECT_EQ(attached_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // Drag the child along the the shelf past the |window|.
  // Its new position should not be overridden.
  DragStart(child.get());
  DragMove(350, 0);
  // While moving the transient child window should be in the panel container.
  EXPECT_EQ(kShellWindowId_PanelContainer, child->parent()->id());
  DragEnd();
  // |child| should move, |window| should not.
  EXPECT_EQ(gfx::Point(20 + 50 + 350, 20 + dy).ToString(),
            child->GetBoundsInScreen().origin().ToString());
  EXPECT_EQ(attached_bounds.ToString(), window->GetBoundsInScreen().ToString());

  DragStart(window.get());
  DragMove(0, -100);
  // While moving the windows should be in the panel container.
  EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
  EXPECT_EQ(kShellWindowId_PanelContainer, child->parent()->id());
  DragEnd();
  // When dropped they should return to the default container.
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  EXPECT_EQ(kShellWindowId_DefaultContainer, child->parent()->id());

  // While moving the window and child should be moved to the panel container.
  DragStart(window.get());
  DragMove(20, 0);
  EXPECT_EQ(kShellWindowId_PanelContainer, window->parent()->id());
  EXPECT_EQ(kShellWindowId_PanelContainer, child->parent()->id());
  DragEnd();

  // When dropped they should return to the default container.
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  EXPECT_EQ(kShellWindowId_DefaultContainer, child->parent()->id());
}

INSTANTIATE_TEST_CASE_P(LtrRtl, PanelWindowResizerTextDirectionTest,
                        testing::Bool());
INSTANTIATE_TEST_CASE_P(NormalPanelPopup,
                        PanelWindowResizerTransientTest,
                        testing::Values(ui::wm::WINDOW_TYPE_NORMAL,
                                        ui::wm::WINDOW_TYPE_PANEL,
                                        ui::wm::WINDOW_TYPE_POPUP));

}  // namespace ash
