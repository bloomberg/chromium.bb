// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/compact_layout_manager.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const int kMaxWidth = 800;
const int kMaxHeight = 600;

}  // namespace

namespace internal {

class CompactLayoutManagerTest : public ash::test::AshTestBase {
 public:
  CompactLayoutManagerTest() : layout_manager_(NULL) {
  }
  virtual ~CompactLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();
    Shell::GetRootWindow()->Show();
    Shell::GetRootWindow()->SetHostSize(gfx::Size(kMaxWidth, kMaxHeight));
    default_container()->SetBounds(gfx::Rect(0, 0, kMaxWidth, kMaxHeight));
    layout_manager_ = new internal::CompactLayoutManager();
    default_container()->SetLayoutManager(layout_manager_);
    default_container()->Show();
    // Control layer animation stepping.
    default_container()->layer()->GetAnimator()->
        set_disable_timer_for_test(true);
    RunAllPendingInMessageLoop();
  }

  aura::Window* CreateNormalWindow(int id) {
    aura::Window* window = new aura::Window(NULL);
    window->set_id(id);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::Layer::LAYER_TEXTURED);
    window->SetBounds(gfx::Rect(0, 0, kMaxWidth, kMaxHeight));
    window->SetParent(default_container());
    wm::MaximizeWindow(window);
    window->Show();
    RunAllPendingInMessageLoop();
    return window;
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    return aura::test::CreateTestWindowWithBounds(bounds, default_container());
  }

  // Returns widget owned by its parent, so doesn't need scoped_ptr<>.
  views::Widget* CreateTestWidget() {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(11, 22, 33, 44);
    widget->Init(params);
    widget->Show();
    return widget;
  }

  internal::CompactLayoutManager* layout_manager() {
    return layout_manager_;
  }

  aura::Window* default_container() const {
    return ash::Shell::GetInstance()->GetContainer(
        ash::internal::kShellWindowId_DefaultContainer);
  }

  int default_container_layer_width() const {
    return default_container()->layer()->bounds().width();
  }

  ui::Transform default_container_layer_transform() const {
    return default_container()->layer()->GetTargetTransform();
  }

  ui::AnimationContainerElement* animation_element() {
    return default_container()->layer()->GetAnimator();
  }

 protected:
  internal::CompactLayoutManager* layout_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompactLayoutManagerTest);
};

// Tests status area visibility during window maximize and fullscreen.
TEST_F(CompactLayoutManagerTest, StatusAreaVisibility) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  views::Widget* widget = CreateTestWidget();
  layout_manager()->set_status_area_widget(widget);
  EXPECT_TRUE(widget->IsVisible());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(widget->IsVisible());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(widget->IsVisible());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_FALSE(widget->IsVisible());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(widget->IsVisible());
}

#if defined(OS_MACOSX)
#define MAYBE_TransitionTest FAILS_TransitionTest
#else
#define MAYBE_TransitionTest TransitionTest
#endif
TEST_F(CompactLayoutManagerTest, MAYBE_TransitionTest) {
  // Assert on viewport size to be the host size.
  ASSERT_EQ(kMaxWidth, default_container_layer_width());
  // Create 3 windows, check that the layer grow as each one is added
  // to the layout.
  aura::Window* window1 = CreateNormalWindow(0);
  EXPECT_EQ(kMaxWidth, default_container_layer_width());
  aura::Window* window2 = CreateNormalWindow(1);
  EXPECT_EQ(kMaxWidth * 2, default_container_layer_width());
  aura::Window* window3 = CreateNormalWindow(2);
  EXPECT_EQ(kMaxWidth * 3, default_container_layer_width());
  animation_element()->Step(base::TimeTicks::Now() +
                            base::TimeDelta::FromSeconds(1));
  RunAllPendingInMessageLoop();

  // Check laid out position of the windows.
  EXPECT_EQ(0, window1->bounds().x());
  EXPECT_EQ(kMaxWidth, window2->bounds().x());
  EXPECT_EQ(kMaxWidth * 2, window3->bounds().x());

  // Check layer transformation.
  ui::Transform target_transform;
  target_transform.ConcatTranslate(-window3->bounds().x(), 0);
  EXPECT_EQ(target_transform, default_container_layer_transform());
  RunAllPendingInMessageLoop();

  // Check that only one window is visible.
  EXPECT_EQ(window3, layout_manager_->current_window_);
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  // That window disappear, check that we transform the layer, and
  // again only have one window visible.
  window3->Hide();
  animation_element()->Step(base::TimeTicks::Now() +
                            base::TimeDelta::FromSeconds(1));
  ui::Transform target_transform1;
  target_transform1.ConcatTranslate(-window1->bounds().x(), 0);
  EXPECT_EQ(target_transform1, default_container_layer_transform());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  EXPECT_EQ(window1, layout_manager_->current_window_);
}

TEST_F(CompactLayoutManagerTest, SwitchToNextVisibleWindow) {
  // Create 3 windows
  aura::Window* window1 = CreateNormalWindow(0);
  window1->Hide();  // Hide window1 before its layer marked invisible.
  aura::Window* window2 = CreateNormalWindow(1);
  aura::Window* window3 = CreateNormalWindow(2);

  // Check that only window3 is visible.
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  // Hide the current active window.
  window3->Hide();

  // And window2 becomes the current window because window1 is hidden.
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());

  // Show window3 and it becomes the current window.
  window3->Show();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  // Close the current active window.
  delete window3;

  // And window2 becomes active again.
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
}

TEST_F(CompactLayoutManagerTest, CloseAllWindows) {
  // Create 3 windows
  aura::Window* window1 = CreateNormalWindow(0);
  aura::Window* window2 = CreateNormalWindow(1);
  aura::Window* window3 = CreateNormalWindow(2);

  // Check that only window3 is visible.
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  // Close all windows. Note they should not be accessed after here.
  delete window1;
  delete window2;
  delete window3;

  // No current window now.
  EXPECT_EQ(NULL, layout_manager_->current_window_);
}

}  // namespace internal
}  // namespace ash
