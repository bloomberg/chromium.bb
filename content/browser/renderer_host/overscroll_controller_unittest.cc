// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/overscroll_controller.h"

#include <memory>

#include "base/containers/queue.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/test/test_overscroll_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class OverscrollControllerTest : public ::testing::Test {
 protected:
  OverscrollControllerTest() {}
  ~OverscrollControllerTest() override {}

  void SetUp() override {
    delegate_ = std::make_unique<TestOverscrollDelegate>(gfx::Size(400, 300));
    controller_ = std::make_unique<OverscrollController>();
    controller_->set_delegate(delegate_.get());
  }

  void TearDown() override {
    controller_ = nullptr;
    delegate_ = nullptr;
  }

  // Creates and sends a mouse-wheel event to the overscroll controller. Returns
  // |true| if the event is consumed by the overscroll controller.
  bool SimulateMouseWheel(float dx, float dy) {
    DCHECK(!current_event_);
    current_event_ = std::make_unique<blink::WebMouseWheelEvent>(
        SyntheticWebMouseWheelEventBuilder::Build(0, 0, dx, dy, 0, true));
    return controller_->WillHandleEvent(*current_event_);
  }

  // Creates and sends a gesture-scroll-update event to the overscroll
  // controller. Returns |true| if the event is consumed by the overscroll
  // controller.
  bool SimulateGestureScrollUpdate(float dx,
                                   float dy,
                                   blink::WebGestureDevice device) {
    DCHECK(!current_event_);
    current_event_ = std::make_unique<blink::WebGestureEvent>(
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0, device));
    return controller_->WillHandleEvent(*current_event_);
  }

  // Notifies the overscroll controller that the current event is ACKed.
  void SimulateAck(bool processed) {
    DCHECK(current_event_);
    controller_->ReceivedEventACK(*current_event_, processed);
    current_event_ = nullptr;
  }

  TestOverscrollDelegate* delegate() const { return delegate_.get(); }

  OverscrollMode controller_mode() const {
    return controller_->overscroll_mode_;
  }

  OverscrollSource controller_source() const {
    return controller_->overscroll_source_;
  }

 private:
  std::unique_ptr<TestOverscrollDelegate> delegate_;
  std::unique_ptr<OverscrollController> controller_;

  // Keeps track of the last event that has been processed by the overscroll
  // controller which is not yet ACKed. Will be null if no event is processed or
  // the last event is ACKed.
  std::unique_ptr<blink::WebInputEvent> current_event_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollControllerTest);
};

// Tests that if a mouse-wheel is consumed by content before overscroll is
// initiated, overscroll will not initiate anymore.
TEST_F(OverscrollControllerTest, MouseWheelConsumedPreventsOverscroll) {
  // Simulate a mouse-wheel, ACK it as not processed, simulate the corresponding
  // gesture scroll-update event, and ACK it is not processed. Since it is not
  // passing the start threshold, no overscroll should happen.
  EXPECT_FALSE(SimulateMouseWheel(10, 0));
  SimulateAck(false);
  EXPECT_FALSE(
      SimulateGestureScrollUpdate(10, 0, blink::kWebGestureDeviceTouchpad));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // Simulate a mouse-wheel and ACK it as processed. No gesture scroll-update
  // needs to be simulated. Still no overscroll.
  EXPECT_FALSE(SimulateMouseWheel(10, 0));
  SimulateAck(true);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // Simulate a mouse-wheel and the corresponding gesture scroll-update both
  // ACKed as not processed. Although the scroll passes overscroll start
  // threshold, no overscroll should happen since the previous mouse-wheel was
  // marked as processed.
  EXPECT_FALSE(SimulateMouseWheel(100, 0));
  SimulateAck(false);
  EXPECT_FALSE(
      SimulateGestureScrollUpdate(100, 0, blink::kWebGestureDeviceTouchpad));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());
}

}  // namespace content
