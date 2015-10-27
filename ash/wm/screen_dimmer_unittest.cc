// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/screen_dimmer.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/dim_window.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
//#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/test/test_windows.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace test {

class ScreenDimmerTest : public AshTestBase {
 public:
  ScreenDimmerTest() : dimmer_(nullptr) {}
  ~ScreenDimmerTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    dimmer_ = ScreenDimmer::GetForRoot();
  }

  aura::Window* GetDimWindow() {
    return DimWindow::Get(Shell::GetPrimaryRootWindow());
  }

  ui::Layer* GetDimWindowLayer() {
    aura::Window* window = GetDimWindow();
    return window ? window->layer() : nullptr;
  }

 protected:
  ScreenDimmer* dimmer_;  // not owned

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenDimmerTest);
};

TEST_F(ScreenDimmerTest, DimAndUndim) {
  // Don't create a layer until we need to.
  EXPECT_EQ(nullptr, GetDimWindowLayer());
  dimmer_->SetDimming(false);
  EXPECT_EQ(nullptr, GetDimWindowLayer());

  // When we enable dimming, the layer should be created and stacked at the top
  // of the root's children.
  dimmer_->SetDimming(true);
  ASSERT_NE(nullptr, GetDimWindowLayer());
  ui::Layer* root_layer = Shell::GetPrimaryRootWindow()->layer();
  ASSERT_TRUE(!root_layer->children().empty());
  EXPECT_EQ(GetDimWindowLayer(), root_layer->children().back());
  EXPECT_TRUE(GetDimWindowLayer()->visible());
  EXPECT_GT(GetDimWindowLayer()->GetTargetOpacity(), 0.0f);

  // When we disable dimming, the layer should be removed.
  dimmer_->SetDimming(false);
  ASSERT_EQ(nullptr, GetDimWindowLayer());
}

TEST_F(ScreenDimmerTest, ResizeLayer) {
  // The dimming layer should be initially sized to cover the root window.
  dimmer_->SetDimming(true);
  ui::Layer* dimming_layer = GetDimWindowLayer();
  ASSERT_TRUE(dimming_layer != nullptr);
  ui::Layer* root_layer = Shell::GetPrimaryRootWindow()->layer();
  EXPECT_EQ(gfx::Rect(root_layer->bounds().size()).ToString(),
            dimming_layer->bounds().ToString());

  // When we resize the root window, the dimming layer should be resized to
  // match.
  gfx::Rect kNewBounds(400, 300);
  Shell::GetPrimaryRootWindow()->GetHost()->SetBounds(kNewBounds);
  EXPECT_EQ(kNewBounds.ToString(), dimming_layer->bounds().ToString());
}

TEST_F(ScreenDimmerTest, RootDimmer) {
  ScreenDimmer* root_dimmer = ScreenDimmer::GetForRoot();
  // -100 is the magic number for root window.
  EXPECT_EQ(root_dimmer, ScreenDimmer::FindForTest(-100));
  EXPECT_EQ(nullptr, ScreenDimmer::FindForTest(-1));
}

TEST_F(ScreenDimmerTest, DimAtBottom) {
  ScreenDimmer* root_dimmer = ScreenDimmer::GetForRoot();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(1, root_window));
  root_dimmer->SetDimming(true);
  std::vector<aura::Window*>::const_iterator dim_iter =
      std::find(root_window->children().begin(), root_window->children().end(),
                GetDimWindow());
  ASSERT_TRUE(dim_iter != root_window->children().end());
  // Dim layer is at top.
  EXPECT_EQ(*dim_iter, *root_window->children().rbegin());

  root_dimmer->SetDimming(false);
  root_dimmer->set_at_bottom(true);
  root_dimmer->SetDimming(true);

  dim_iter = std::find(root_window->children().begin(),
                       root_window->children().end(), GetDimWindow());
  ASSERT_TRUE(dim_iter != root_window->children().end());
  // Dom layer is at the bottom.
  EXPECT_EQ(*dim_iter, *root_window->children().begin());
}

}  // namespace test
}  // namespace ash
