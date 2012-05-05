// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/screen_dimmer.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace test {

class ScreenDimmerTest : public AshTestBase {
 public:
  ScreenDimmerTest() : dimmer_(NULL) {}
  virtual ~ScreenDimmerTest() {}

  void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    dimmer_ = Shell::GetInstance()->screen_dimmer();
    test_api_.reset(new internal::ScreenDimmer::TestApi(dimmer_));
  }

 protected:
  internal::ScreenDimmer* dimmer_;  // not owned

  scoped_ptr<internal::ScreenDimmer::TestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenDimmerTest);
};

TEST_F(ScreenDimmerTest, DimAndUndim) {
  // Don't create a layer until we need to.
  EXPECT_TRUE(test_api_->layer() == NULL);
  dimmer_->SetDimming(false);
  EXPECT_TRUE(test_api_->layer() == NULL);

  // When we enable dimming, the layer should be created and stacked at the top
  // of the root's children.
  dimmer_->SetDimming(true);
  ASSERT_TRUE(test_api_->layer() != NULL);
  ui::Layer* root_layer = Shell::GetInstance()->GetRootWindow()->layer();
  ASSERT_TRUE(!root_layer->children().empty());
  EXPECT_EQ(test_api_->layer(), root_layer->children().back());
  EXPECT_TRUE(test_api_->layer()->visible());
  EXPECT_GT(test_api_->layer()->GetTargetOpacity(), 0.0f);

  // When we disable dimming, the layer should be animated back to full
  // transparency.
  dimmer_->SetDimming(false);
  ASSERT_TRUE(test_api_->layer() != NULL);
  EXPECT_TRUE(test_api_->layer()->visible());
  EXPECT_FLOAT_EQ(0.0f, test_api_->layer()->GetTargetOpacity());
}

TEST_F(ScreenDimmerTest, ResizeLayer) {
  // The dimming layer should be initially sized to cover the root window.
  dimmer_->SetDimming(true);
  ui::Layer* dimming_layer = test_api_->layer();
  ASSERT_TRUE(dimming_layer != NULL);
  ui::Layer* root_layer = Shell::GetInstance()->GetRootWindow()->layer();
  EXPECT_EQ(gfx::Rect(root_layer->bounds().size()).ToString(),
            dimming_layer->bounds().ToString());

  // When we resize the root window, the dimming layer should be resized to
  // match.
  gfx::Size kNewSize(400, 300);
  Shell::GetInstance()->GetRootWindow()->SetHostSize(kNewSize);
  EXPECT_EQ(kNewSize.ToString(), dimming_layer->bounds().size().ToString());
}

}  // namespace test
}  // namespace ash
