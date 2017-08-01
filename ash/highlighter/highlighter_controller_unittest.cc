// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_controller.h"

#include "ash/highlighter/highlighter_controller_test_api.h"
#include "ash/highlighter/highlighter_view.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

class HighlighterControllerTest : public AshTestBase {
 public:
  HighlighterControllerTest() {}
  ~HighlighterControllerTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = base::MakeUnique<HighlighterController>();
  }

  void TearDown() override {
    // This needs to be called first to remove the event handler before the
    // shell instance gets torn down.
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<HighlighterController> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HighlighterControllerTest);
};

}  // namespace

// Test to ensure the class responsible for drawing the highlighter pointer
// receives points from stylus movements as expected.
TEST_F(HighlighterControllerTest, HighlighterRenderer) {
  HighlighterControllerTestApi controller_test_api_(controller_.get());

  // The highlighter pointer mode only works with stylus.
  GetEventGenerator().EnterPenPointerMode();

  // When disabled the highlighter pointer should not be showing.
  GetEventGenerator().MoveTouch(gfx::Point(1, 1));
  EXPECT_FALSE(controller_test_api_.IsShowingHighlighter());

  // Verify that by enabling the mode, the highlighter pointer should still not
  // be showing.
  controller_test_api_.SetEnabled(true);
  EXPECT_FALSE(controller_test_api_.IsShowingHighlighter());

  // Verify moving the stylus 4 times will not display the highlighter pointer.
  GetEventGenerator().MoveTouch(gfx::Point(2, 2));
  GetEventGenerator().MoveTouch(gfx::Point(3, 3));
  GetEventGenerator().MoveTouch(gfx::Point(4, 4));
  GetEventGenerator().MoveTouch(gfx::Point(5, 5));
  EXPECT_FALSE(controller_test_api_.IsShowingHighlighter());

  // Verify pressing the stylus will show the highlighter pointer and add a
  // point but will not activate fading out.
  GetEventGenerator().PressTouch();
  EXPECT_TRUE(controller_test_api_.IsShowingHighlighter());
  EXPECT_FALSE(controller_test_api_.IsFadingAway());
  EXPECT_EQ(1, controller_test_api_.points().GetNumberOfPoints());

  // Verify dragging the stylus 2 times will add 2 more points.
  GetEventGenerator().MoveTouch(gfx::Point(6, 6));
  GetEventGenerator().MoveTouch(gfx::Point(7, 7));
  EXPECT_EQ(3, controller_test_api_.points().GetNumberOfPoints());

  // Verify releasing the stylus still shows the highlighter pointer, which is
  // fading away.
  GetEventGenerator().ReleaseTouch();
  EXPECT_TRUE(controller_test_api_.IsShowingHighlighter());
  EXPECT_TRUE(controller_test_api_.IsFadingAway());

  // Verify that disabling the mode does not display the highlighter pointer.
  controller_test_api_.SetEnabled(false);
  EXPECT_FALSE(controller_test_api_.IsShowingHighlighter());
  EXPECT_FALSE(controller_test_api_.IsFadingAway());

  // Verify that disabling the mode while highlighter pointer is displayed does
  // not display the highlighter pointer.
  controller_test_api_.SetEnabled(true);
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(6, 6));
  EXPECT_TRUE(controller_test_api_.IsShowingHighlighter());
  controller_test_api_.SetEnabled(false);
  EXPECT_FALSE(controller_test_api_.IsShowingHighlighter());

  // Verify that the highlighter pointer does not add points while disabled.
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(8, 8));
  GetEventGenerator().ReleaseTouch();
  GetEventGenerator().MoveTouch(gfx::Point(9, 9));
  EXPECT_FALSE(controller_test_api_.IsShowingHighlighter());

  // Verify that the highlighter pointer does not get shown if points are not
  // coming from the stylus, even when enabled.
  GetEventGenerator().ExitPenPointerMode();
  controller_test_api_.SetEnabled(true);
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(10, 10));
  GetEventGenerator().MoveTouch(gfx::Point(11, 11));
  EXPECT_FALSE(controller_test_api_.IsShowingHighlighter());
  GetEventGenerator().ReleaseTouch();
}

// Test to ensure the class responsible for drawing the highlighter pointer
// handles prediction as expected when it receives points from stylus movements.
TEST_F(HighlighterControllerTest, HighlighterPrediction) {
  HighlighterControllerTestApi controller_test_api_(controller_.get());

  controller_test_api_.SetEnabled(true);
  // The highlighter pointer mode only works with stylus.
  GetEventGenerator().EnterPenPointerMode();
  GetEventGenerator().PressTouch();
  EXPECT_TRUE(controller_test_api_.IsShowingHighlighter());

  EXPECT_EQ(1, controller_test_api_.points().GetNumberOfPoints());
  // Initial press event shouldn't generate any predicted points as there's no
  // history to use for prediction.
  EXPECT_EQ(0, controller_test_api_.predicted_points().GetNumberOfPoints());

  // Verify dragging the stylus 3 times will add some predicted points.
  GetEventGenerator().MoveTouch(gfx::Point(10, 10));
  GetEventGenerator().MoveTouch(gfx::Point(20, 20));
  GetEventGenerator().MoveTouch(gfx::Point(30, 30));
  EXPECT_NE(0, controller_test_api_.predicted_points().GetNumberOfPoints());
  // Verify predicted points are in the right direction.
  for (const auto& point : controller_test_api_.predicted_points().points()) {
    EXPECT_LT(30, point.location.x());
    EXPECT_LT(30, point.location.y());
  }
}

}  // namespace ash
