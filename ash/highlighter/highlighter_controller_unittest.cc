// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_controller.h"

#include "ash/fast_ink/fast_ink_points.h"
#include "ash/highlighter/highlighter_controller_test_api.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/stringprintf.h"
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
    controller_test_api_ =
        base::MakeUnique<HighlighterControllerTestApi>(controller_.get());
  }

  void TearDown() override {
    // This needs to be called first to remove the event handler before the
    // shell instance gets torn down.
    controller_test_api_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void TraceRect(const gfx::Rect& rect) {
    GetEventGenerator().MoveTouch(gfx::Point(rect.x(), rect.y()));
    GetEventGenerator().PressTouch();
    GetEventGenerator().MoveTouch(gfx::Point(rect.right(), rect.y()));
    GetEventGenerator().MoveTouch(gfx::Point(rect.right(), rect.bottom()));
    GetEventGenerator().MoveTouch(gfx::Point(rect.x(), rect.bottom()));
    GetEventGenerator().MoveTouch(gfx::Point(rect.x(), rect.y()));
    GetEventGenerator().ReleaseTouch();
  }

  std::unique_ptr<HighlighterController> controller_;
  std::unique_ptr<HighlighterControllerTestApi> controller_test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HighlighterControllerTest);
};

}  // namespace

// Test to ensure the class responsible for drawing the highlighter pointer
// receives points from stylus movements as expected.
TEST_F(HighlighterControllerTest, HighlighterRenderer) {
  // The highlighter pointer mode only works with stylus.
  GetEventGenerator().EnterPenPointerMode();

  // When disabled the highlighter pointer should not be showing.
  GetEventGenerator().MoveTouch(gfx::Point(1, 1));
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify that by enabling the mode, the highlighter pointer should still not
  // be showing.
  controller_test_api_->SetEnabled(true);
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify moving the stylus 4 times will not display the highlighter pointer.
  GetEventGenerator().MoveTouch(gfx::Point(2, 2));
  GetEventGenerator().MoveTouch(gfx::Point(3, 3));
  GetEventGenerator().MoveTouch(gfx::Point(4, 4));
  GetEventGenerator().MoveTouch(gfx::Point(5, 5));
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify pressing the stylus will show the highlighter pointer and add a
  // point but will not activate fading out.
  GetEventGenerator().PressTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());
  EXPECT_FALSE(controller_test_api_->IsFadingAway());
  EXPECT_EQ(1, controller_test_api_->points().GetNumberOfPoints());

  // Verify dragging the stylus 2 times will add 2 more points.
  GetEventGenerator().MoveTouch(gfx::Point(6, 6));
  GetEventGenerator().MoveTouch(gfx::Point(7, 7));
  EXPECT_EQ(3, controller_test_api_->points().GetNumberOfPoints());

  // Verify releasing the stylus still shows the highlighter pointer, which is
  // fading away.
  GetEventGenerator().ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());
  EXPECT_TRUE(controller_test_api_->IsFadingAway());

  // Verify that disabling the mode right after the gesture completion does not
  // hide the highlighter pointer immediately but lets it play out the
  // animation.
  controller_test_api_->SetEnabled(false);
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());
  EXPECT_TRUE(controller_test_api_->IsFadingAway());

  // Verify that disabling the mode mid-gesture hides the highlighter pointer
  // immediately.
  controller_test_api_->DestroyPointerView();
  controller_test_api_->SetEnabled(true);
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(6, 6));
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());
  controller_test_api_->SetEnabled(false);
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify that the highlighter pointer does not add points while disabled.
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(8, 8));
  GetEventGenerator().ReleaseTouch();
  GetEventGenerator().MoveTouch(gfx::Point(9, 9));
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify that the highlighter pointer does not get shown if points are not
  // coming from the stylus, even when enabled.
  GetEventGenerator().ExitPenPointerMode();
  controller_test_api_->SetEnabled(true);
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(10, 10));
  GetEventGenerator().MoveTouch(gfx::Point(11, 11));
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());
  GetEventGenerator().ReleaseTouch();
}

// Test to ensure the class responsible for drawing the highlighter pointer
// handles prediction as expected when it receives points from stylus movements.
TEST_F(HighlighterControllerTest, HighlighterPrediction) {
  controller_test_api_->SetEnabled(true);
  // The highlighter pointer mode only works with stylus.
  GetEventGenerator().EnterPenPointerMode();
  GetEventGenerator().PressTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());

  EXPECT_EQ(1, controller_test_api_->points().GetNumberOfPoints());
  // Initial press event shouldn't generate any predicted points as there's no
  // history to use for prediction.
  EXPECT_EQ(0, controller_test_api_->predicted_points().GetNumberOfPoints());

  // Verify dragging the stylus 3 times will add some predicted points.
  GetEventGenerator().MoveTouch(gfx::Point(10, 10));
  GetEventGenerator().MoveTouch(gfx::Point(20, 20));
  GetEventGenerator().MoveTouch(gfx::Point(30, 30));
  EXPECT_NE(0, controller_test_api_->predicted_points().GetNumberOfPoints());
  // Verify predicted points are in the right direction.
  for (const auto& point : controller_test_api_->predicted_points().points()) {
    EXPECT_LT(30, point.location.x());
    EXPECT_LT(30, point.location.y());
  }
}

// Test that stylus gestures are correctly recognized by HighlighterController.
TEST_F(HighlighterControllerTest, HighlighterGestures) {
  controller_test_api_->SetEnabled(true);
  GetEventGenerator().EnterPenPointerMode();

  // A non-horizontal stroke is not recognized
  controller_test_api_->ResetSelection();
  GetEventGenerator().MoveTouch(gfx::Point(100, 100));
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(200, 200));
  GetEventGenerator().ReleaseTouch();
  EXPECT_FALSE(controller_test_api_->handle_selection_called());
  EXPECT_TRUE(controller_test_api_->handle_failed_selection_called());

  // An almost horizontal stroke is recognized
  controller_test_api_->ResetSelection();
  GetEventGenerator().MoveTouch(gfx::Point(100, 100));
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(300, 102));
  GetEventGenerator().ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());

  // Horizontal stroke selection rectangle should:
  //   have the same horizontal center line as the stroke bounding box,
  //   be 4dp wider than the stroke bounding box,
  //   be exactly 14dp high.
  EXPECT_EQ("98,94 204x14", controller_test_api_->selection().ToString());

  // An insufficiently closed C-like shape is not recognized
  controller_test_api_->ResetSelection();
  GetEventGenerator().MoveTouch(gfx::Point(100, 0));
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(0, 0));
  GetEventGenerator().MoveTouch(gfx::Point(0, 100));
  GetEventGenerator().MoveTouch(gfx::Point(100, 100));
  GetEventGenerator().ReleaseTouch();
  EXPECT_FALSE(controller_test_api_->handle_selection_called());
  EXPECT_TRUE(controller_test_api_->handle_failed_selection_called());

  // An almost closed G-like shape is recognized
  controller_test_api_->ResetSelection();
  GetEventGenerator().MoveTouch(gfx::Point(200, 0));
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(0, 0));
  GetEventGenerator().MoveTouch(gfx::Point(0, 100));
  GetEventGenerator().MoveTouch(gfx::Point(200, 100));
  GetEventGenerator().MoveTouch(gfx::Point(200, 20));
  GetEventGenerator().ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_EQ("0,0 200x100", controller_test_api_->selection().ToString());

  // A closed diamond shape is recognized
  controller_test_api_->ResetSelection();
  GetEventGenerator().MoveTouch(gfx::Point(100, 50));
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(200, 150));
  GetEventGenerator().MoveTouch(gfx::Point(100, 250));
  GetEventGenerator().MoveTouch(gfx::Point(0, 150));
  GetEventGenerator().MoveTouch(gfx::Point(100, 50));
  GetEventGenerator().ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_EQ("0,50 200x200", controller_test_api_->selection().ToString());
}

// Test that stylus gesture recognition correctly handles display scaling
TEST_F(HighlighterControllerTest, HighlighterGesturesScaled) {
  controller_test_api_->SetEnabled(true);
  GetEventGenerator().EnterPenPointerMode();

  const gfx::Rect original_rect(200, 100, 400, 300);

  // Allow for rounding errors.
  gfx::Rect inflated(original_rect);
  inflated.Inset(-1, -1);

  constexpr float display_scales[] = {1.f, 1.5f, 2.0f};
  constexpr float ui_scales[] = {0.5f,  0.67f, 1.0f,  1.25f,
                                 1.33f, 1.5f,  1.67f, 2.0f};

  for (size_t i = 0; i < sizeof(display_scales) / sizeof(float); ++i) {
    const float display_scale = display_scales[i];
    for (size_t j = 0; j < sizeof(ui_scales) / sizeof(float); ++j) {
      const float ui_scale = ui_scales[j];

      std::string display_spec =
          base::StringPrintf("1500x1000*%.2f@%.2f", display_scale, ui_scale);
      SCOPED_TRACE(display_spec);
      UpdateDisplay(display_spec);

      controller_test_api_->ResetSelection();
      TraceRect(original_rect);
      EXPECT_TRUE(controller_test_api_->handle_selection_called());
      EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());

      const gfx::Rect selection = controller_test_api_->selection();
      EXPECT_TRUE(inflated.Contains(selection));
      EXPECT_TRUE(selection.Contains(original_rect));
    }
  }
}

// Test that stylus gesture recognition correctly handles display rotation
TEST_F(HighlighterControllerTest, HighlighterGesturesRotated) {
  controller_test_api_->SetEnabled(true);
  GetEventGenerator().EnterPenPointerMode();

  const gfx::Rect trace(200, 100, 400, 300);

  // No rotation
  UpdateDisplay("1500x1000");
  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_TRUE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_EQ("200,100 400x300", controller_test_api_->selection().ToString());

  // Rotate to 90 degrees
  UpdateDisplay("1500x1000/r");
  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_TRUE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_EQ("100,900 300x400", controller_test_api_->selection().ToString());

  // Rotate to 180 degrees
  UpdateDisplay("1500x1000/u");
  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_TRUE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_EQ("900,600 400x300", controller_test_api_->selection().ToString());

  // Rotate to 270 degrees
  UpdateDisplay("1500x1000/l");
  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_TRUE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_EQ("600,200 300x400", controller_test_api_->selection().ToString());
}

// Test that a stroke interrupted close to the screen edge is treated as
// contiguous.
TEST_F(HighlighterControllerTest, InterruptedStroke) {
  controller_test_api_->SetEnabled(true);
  GetEventGenerator().EnterPenPointerMode();

  UpdateDisplay("1500x1000");

  // An interrupted stroke close to the screen edge should be recognized as a
  // contiguous stroke.
  controller_test_api_->ResetSelection();
  GetEventGenerator().MoveTouch(gfx::Point(300, 100));
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(0, 100));
  GetEventGenerator().ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->IsWaitingToResumeStroke());
  EXPECT_FALSE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_FALSE(controller_test_api_->IsFadingAway());

  GetEventGenerator().MoveTouch(gfx::Point(0, 200));
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(300, 200));
  GetEventGenerator().ReleaseTouch();
  EXPECT_FALSE(controller_test_api_->IsWaitingToResumeStroke());
  EXPECT_TRUE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_EQ("0,100 300x100", controller_test_api_->selection().ToString());

  // Repeat the same gesture, but simulate a timeout after the gap. This should
  // force the gesture completion.
  controller_test_api_->ResetSelection();
  GetEventGenerator().MoveTouch(gfx::Point(300, 100));
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(0, 100));
  GetEventGenerator().ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->IsWaitingToResumeStroke());
  EXPECT_FALSE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_FALSE(controller_test_api_->IsFadingAway());

  controller_test_api_->SimulateInterruptedStrokeTimeout();
  EXPECT_FALSE(controller_test_api_->IsWaitingToResumeStroke());
  EXPECT_TRUE(controller_test_api_->handle_selection_called());
  EXPECT_FALSE(controller_test_api_->handle_failed_selection_called());
  EXPECT_TRUE(controller_test_api_->IsFadingAway());
}

// Test that the selection is never crossing the screen bounds.
TEST_F(HighlighterControllerTest, SelectionInsideScreen) {
  controller_test_api_->SetEnabled(true);
  GetEventGenerator().EnterPenPointerMode();

  constexpr float display_scales[] = {1.f, 1.5f, 2.0f};

  for (size_t i = 0; i < sizeof(display_scales) / sizeof(float); ++i) {
    std::string display_spec =
        base::StringPrintf("1000x1000*%.2f", display_scales[i]);
    SCOPED_TRACE(display_spec);
    UpdateDisplay(display_spec);

    const gfx::Rect screen(0, 0, 1000, 1000);

    // Rectangle completely offscreen.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(-100, -100, 10, 10));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->handle_failed_selection_called());

    // Rectangle crossing the left edge.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(-100, 100, 200, 200));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->handle_selection_called());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Rectangle crossing the top edge.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(100, -100, 200, 200));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->handle_selection_called());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Rectangle crossing the right edge.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(900, 100, 200, 200));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->handle_selection_called());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Rectangle crossing the bottom edge.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(100, 900, 200, 200));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->handle_selection_called());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Horizontal stroke completely offscreen.
    controller_test_api_->ResetSelection();
    GetEventGenerator().MoveTouch(gfx::Point(0, -100));
    GetEventGenerator().PressTouch();
    GetEventGenerator().MoveTouch(gfx::Point(1000, -100));
    GetEventGenerator().ReleaseTouch();
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->handle_failed_selection_called());

    // Horizontal stroke along the top edge of the screen.
    controller_test_api_->ResetSelection();
    GetEventGenerator().MoveTouch(gfx::Point(0, 0));
    GetEventGenerator().PressTouch();
    GetEventGenerator().MoveTouch(gfx::Point(1000, 0));
    GetEventGenerator().ReleaseTouch();
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->handle_selection_called());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Horizontal stroke along the bottom edge of the screen.
    controller_test_api_->ResetSelection();
    GetEventGenerator().MoveTouch(gfx::Point(0, 999));
    GetEventGenerator().PressTouch();
    GetEventGenerator().MoveTouch(gfx::Point(1000, 999));
    GetEventGenerator().ReleaseTouch();
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->handle_selection_called());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));
  }
}

}  // namespace ash
