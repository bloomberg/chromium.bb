// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/compact_layout_manager.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class CompactLayoutManagerTest : public aura::test::AuraTestBase {
 public:
  CompactLayoutManagerTest() : layout_manager_(NULL) {}
  virtual ~CompactLayoutManagerTest() {}

  internal::CompactLayoutManager* layout_manager() {
    return layout_manager_;
  }

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    aura::RootWindow::GetInstance()->screen()->set_work_area_insets(
        gfx::Insets(1, 2, 3, 4));
    aura::RootWindow::GetInstance()->SetHostSize(gfx::Size(800, 600));
    container_.reset(new aura::Window(NULL));
    container_->Init(ui::Layer::LAYER_NOT_DRAWN);
    container_->SetBounds(gfx::Rect(0, 0, 500, 500));
    layout_manager_ = new internal::CompactLayoutManager();
    container_->SetLayoutManager(layout_manager_);
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    return aura::test::CreateTestWindowWithBounds(bounds, container_.get());
  }

  // Returns widget owned by its parent, so doesn't need scoped_ptr<>.
  views::Widget* CreateTestWidget() {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
    params.bounds = gfx::Rect(11, 22, 33, 44);
    widget->Init(params);
    widget->Show();
    return widget;
  }

 private:
  // Owned by |container_|.
  internal::CompactLayoutManager* layout_manager_;

  scoped_ptr<aura::Window> container_;

  DISALLOW_COPY_AND_ASSIGN(CompactLayoutManagerTest);
};

}  // namespace

// Tests status area visibility during window maximize and fullscreen.
TEST_F(CompactLayoutManagerTest, StatusAreaVisibility) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  views::Widget* widget = CreateTestWidget();
  layout_manager()->set_status_area_widget(widget);
  EXPECT_TRUE(widget->IsVisible());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(widget->IsVisible());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(widget->IsVisible());
  window->SetIntProperty(aura::client::kShowStateKey,
                         ui::SHOW_STATE_FULLSCREEN);
  EXPECT_FALSE(widget->IsVisible());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(widget->IsVisible());
}

namespace {

const int kMaxWidth = 800;
const int kMaxHeight = 600;

}  // namespace

namespace internal {

class CompactLayoutManagerTransitionTest : public aura::test::AuraTestBase {
 public:
  CompactLayoutManagerTransitionTest() : layout_manager_(NULL) {
  }
  virtual ~CompactLayoutManagerTransitionTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    ash::Shell::CreateInstance(new ash::test::TestShellDelegate);
    aura::RootWindow::GetInstance()->Show();
    aura::RootWindow::GetInstance()->SetHostSize(
        gfx::Size(kMaxWidth, kMaxHeight));
    default_container()->SetBounds(gfx::Rect(0, 0, kMaxWidth, kMaxHeight));
    layout_manager_ = new internal::CompactLayoutManager();
    default_container()->SetLayoutManager(layout_manager_);
    default_container()->Show();
    // Control layer animation stepping.
    default_container()->layer()->GetAnimator()->
        set_disable_timer_for_test(true);
  }

  virtual void TearDown() OVERRIDE {
    ash::Shell::DeleteInstance();
    aura::test::AuraTestBase::TearDown();
  }

  aura::Window* CreateNormalWindow(int id) {
    aura::Window* window = new aura::Window(NULL);
    window->set_id(id);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::Layer::LAYER_TEXTURED);
    window->SetBounds(gfx::Rect(0, 0, kMaxWidth, kMaxHeight));
    window->SetParent(default_container());
    window_util::MaximizeWindow(window);
    window->Show();
    RunAllPendingInMessageLoop();
    return window;
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
  DISALLOW_COPY_AND_ASSIGN(CompactLayoutManagerTransitionTest);
};

TEST_F(CompactLayoutManagerTransitionTest, TransitionTest) {
  // Assert on viewport size to be the host size.
  ASSERT_EQ(kMaxWidth, default_container_layer_width());
  // Create 3 windows, check that the layer grow as each one is added
  // to the layout.
  aura::Window* window1 = CreateNormalWindow(0);
  EXPECT_EQ(kMaxWidth, default_container_layer_width());
  aura::Window* window2 =  CreateNormalWindow(1);
  EXPECT_EQ(kMaxWidth * 2, default_container_layer_width());
  aura::Window* window3 =  CreateNormalWindow(2);
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

}  // namespace internal
}  // namespace ash
