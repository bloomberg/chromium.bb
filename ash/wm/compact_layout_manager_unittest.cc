// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/compact_layout_manager.h"

#include "ash/wm/shelf_layout_manager.h"
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
    container_->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
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

}  // namespace ash
