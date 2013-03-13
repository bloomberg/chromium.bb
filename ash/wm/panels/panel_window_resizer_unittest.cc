// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_window_resizer.h"

#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

class PanelWindowResizerTest : public test::AshTestBase {
 public:
  PanelWindowResizerTest() : window_(NULL) {}
  virtual ~PanelWindowResizerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    window_.reset(CreatePanelWindow(gfx::Rect(0, 0, 201, 201)));

    panel_layout_manager_ = static_cast<internal::PanelLayoutManager*>(
        GetPanelContainer()->layout_manager());
    launcher_bounds_ = panel_layout_manager_->launcher()->shelf_widget()->
        GetWindowBoundsInScreen();
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  gfx::Point CalculateDragPoint(const PanelWindowResizer& resizer,
                                int delta_x,
                                int delta_y) const {
    gfx::Point location = resizer.GetInitialLocationInParentForTest();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  aura::Window* CreatePanelWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL,
        aura::client::WINDOW_TYPE_PANEL,
        0,
        bounds);
    test::TestLauncherDelegate* launcher_delegate =
        test::TestLauncherDelegate::instance();
    launcher_delegate->AddLauncherItem(window);
    PanelLayoutManager* manager =
        static_cast<PanelLayoutManager*>(GetPanelContainer()->layout_manager());
    manager->Relayout();
    return window;
  }

  aura::Window* GetPanelContainer() {
    return Shell::GetContainer(
        Shell::GetPrimaryRootWindow(),
        internal::kShellWindowId_PanelContainer);
  }

  static PanelWindowResizer* CreatePanelWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return static_cast<PanelWindowResizer*>(CreateWindowResizer(
        window, point_in_parent, window_component).release());
  }

  void DragStart() {
    resizer_.reset(CreatePanelWindowResizer(window_.get(), gfx::Point(),
                                            HTCAPTION));
    ASSERT_TRUE(resizer_.get());
  }

  void DragMove(int dx, int dy) {
    resizer_->Drag(CalculateDragPoint(*resizer_, dx, dy), 0);
  }

  void DragEnd() {
    resizer_->CompleteDrag(0);
    resizer_.reset();
  }

  // Test dragging the panel slightly, then detaching, and then reattaching
  // dragging out by the vector (dx, dy).
  void DetachReattachTest(int dx, int dy) {
    EXPECT_TRUE(window_->GetProperty(kPanelAttachedKey));
    DragStart();
    gfx::Rect initial_bounds = window_->bounds();

    // Drag the panel slightly. The window should still be snapped to the
    // launcher.
    DragMove(dx * 5, dy * 5);
    EXPECT_EQ(initial_bounds.x(), window_->bounds().x());
    EXPECT_EQ(initial_bounds.y(), window_->bounds().y());

    // Drag further out and the window should now move to the cursor.
    DragMove(dx * 100, dy * 100);
    EXPECT_EQ(initial_bounds.x() + dx * 100, window_->bounds().x());
    EXPECT_EQ(initial_bounds.y() + dy * 100, window_->bounds().y());

    // The panel should be detached when the drag completes.
    DragEnd();
    EXPECT_FALSE(window_->GetProperty(kPanelAttachedKey));

    DragStart();
    // Drag the panel down.
    DragMove(dx * -95, dy * -95);
    // Release the mouse and the panel should be reattached.
    DragEnd();

    // The panel should be reattached and have snapped to the launcher.
    EXPECT_TRUE(window_->GetProperty(kPanelAttachedKey));
    EXPECT_EQ(initial_bounds.x(), window_->bounds().x());
    EXPECT_EQ(initial_bounds.y(), window_->bounds().y());
  }

  aura::test::TestWindowDelegate delegate_;
  scoped_ptr<aura::Window> window_;
  scoped_ptr<PanelWindowResizer> resizer_;
  aura::Window* panel_container_;
  internal::PanelLayoutManager* panel_layout_manager_;
  gfx::Rect launcher_bounds_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PanelWindowResizerTest);
};

// Verifies a window can be dragged from the panel and detached and then
// reattached.
TEST_F(PanelWindowResizerTest, PanelDetachReattachBottom) {
  DetachReattachTest(0, -1);
}

TEST_F(PanelWindowResizerTest, PanelDetachReattachLeft) {
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_LEFT, shell->GetPrimaryRootWindow());
  DetachReattachTest(1, 0);
}

#if defined(OS_WIN)
// TODO(flackr): Positioning of the panel seems to be off on Windows Aura when
// attached to the right (http://crbug.com/180892).
#define MAYBE_PanelDetachReattachRight DISABLED_PanelDetachReattachRight
#else
#define MAYBE_PanelDetachReattachRight PanelDetachReattachRight
#endif

TEST_F(PanelWindowResizerTest, MAYBE_PanelDetachReattachRight) {
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_RIGHT,
                           shell->GetPrimaryRootWindow());
  DetachReattachTest(-1, 0);
}

TEST_F(PanelWindowResizerTest, PanelDetachReattachTop) {
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_TOP, shell->GetPrimaryRootWindow());
  DetachReattachTest(0, 1);
}

}  // namespace internal
}  // namespace ash
