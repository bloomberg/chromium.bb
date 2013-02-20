// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panel_window_resizer.h"

#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/wm/panel_layout_manager.h"
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
    launcher_top_ = panel_layout_manager_->launcher()->widget()->
        GetWindowBoundsInScreen().y();
    EXPECT_EQ(launcher_top_, window_->bounds().bottom());
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

  aura::test::TestWindowDelegate delegate_;
  scoped_ptr<aura::Window> window_;
  aura::Window* panel_container_;
  internal::PanelLayoutManager* panel_layout_manager_;
  int launcher_top_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PanelWindowResizerTest);
};

// Verifies a window can be dragged from the panel and detached and then
// reattached.
TEST_F(PanelWindowResizerTest, PanelDetachReattach) {
  EXPECT_TRUE(window_->GetProperty(kPanelAttachedKey));
  {
    scoped_ptr<PanelWindowResizer> resizer(CreatePanelWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    gfx::Rect initial_bounds = window_->bounds();

    // Drag the panel up slightly. The window should still be snapped to the
    // launcher.
    resizer->Drag(CalculateDragPoint(*resizer, 0, -5), 0);
    EXPECT_EQ(initial_bounds.y(), window_->bounds().y());

    // Drag further up and the window should now move to the cursor.
    resizer->Drag(CalculateDragPoint(*resizer, 0, -100), 0);
    EXPECT_EQ(initial_bounds.y() - 100, window_->bounds().y());

    resizer->CompleteDrag(0);
  }
  // The panel should be detached.
  EXPECT_FALSE(window_->GetProperty(kPanelAttachedKey));

  {
    scoped_ptr<PanelWindowResizer> resizer(CreatePanelWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    gfx::Rect initial_bounds = window_->bounds();

    // Drag the panel down.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 95), 0);
    // Release the mouse and the panel should be reattached.
    resizer->CompleteDrag(0);
  }
  // The panel should be reattached and have snapped to the launcher.
  EXPECT_TRUE(window_->GetProperty(kPanelAttachedKey));
  EXPECT_EQ(launcher_top_, window_->bounds().bottom());
}

}  // namespace internal
}  // namespace ash
