// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/tap_suppression_controller.h"
#include "content/browser/renderer_host/input/tap_suppression_controller_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace content {

class MockTapSuppressionController : public TapSuppressionController,
                                     public TapSuppressionControllerClient {
 public:
  using TapSuppressionController::NOTHING;
  using TapSuppressionController::GFC_IN_PROGRESS;
  using TapSuppressionController::TAP_DOWN_STASHED;
  using TapSuppressionController::LAST_CANCEL_STOPPED_FLING;

  enum Action {
    NONE                                 = 0,
    TAP_DOWN_DEFERRED                    = 1 << 0,
    TAP_DOWN_FORWARDED                   = 1 << 1,
    TAP_DOWN_DROPPED                     = 1 << 2,
    TAP_UP_SUPPRESSED                    = 1 << 3,
    TAP_UP_FORWARDED                     = 1 << 4,
    TAP_CANCEL_SUPPRESSED                = 1 << 5,
    TAP_CANCEL_FORWARDED                 = 1 << 6,
    STASHED_TAP_DOWN_FORWARDED           = 1 << 7,
  };

  MockTapSuppressionController()
      : TapSuppressionController(this),
        max_cancel_to_down_time_in_ms_(1),
        max_tap_gap_time_in_ms_(1),
        last_actions_(NONE),
        time_(),
        timer_started_(false) {
  }

  virtual ~MockTapSuppressionController() {}

  void SendGestureFlingCancel() {
    last_actions_ = NONE;
    GestureFlingCancel();
  }

  void SendGestureFlingCancelAck(bool processed) {
    last_actions_ = NONE;
    GestureFlingCancelAck(processed);
  }

  void SendTapDown() {
    last_actions_ = NONE;
    if (ShouldDeferTapDown())
      last_actions_ |= TAP_DOWN_DEFERRED;
    else
      last_actions_ |= TAP_DOWN_FORWARDED;
  }

  void SendTapUp() {
    last_actions_ = NONE;
    if (ShouldSuppressTapEnd())
      last_actions_ |= TAP_UP_SUPPRESSED;
    else
      last_actions_ |= TAP_UP_FORWARDED;
  }

  void SendTapCancel() {
    last_actions_ = NONE;
    if (ShouldSuppressTapEnd())
      last_actions_ |= TAP_CANCEL_SUPPRESSED;
    else
      last_actions_ |= TAP_CANCEL_FORWARDED;
  }

  void AdvanceTime(const base::TimeDelta& delta) {
    last_actions_ = NONE;
    time_ += delta;
    if (timer_started_ && time_ >= timer_expiry_time_) {
      timer_started_ = false;
      TapDownTimerExpired();
    }
  }

  void set_max_cancel_to_down_time_in_ms(int val) {
    max_cancel_to_down_time_in_ms_ = val;
  }

  void set_max_tap_gap_time_in_ms(int val) {
    max_tap_gap_time_in_ms_ = val;
  }

  State state() { return state_; }

  int last_actions() { return last_actions_; }

 protected:
  virtual base::TimeTicks Now() OVERRIDE {
    return time_;
  }

  virtual void StartTapDownTimer(const base::TimeDelta& delay) OVERRIDE {
    timer_expiry_time_ = time_ + delay;
    timer_started_ = true;
  }

  virtual void StopTapDownTimer() OVERRIDE {
    timer_started_ = false;
  }

 private:
  // TapSuppressionControllerClient implementation
  virtual int MaxCancelToDownTimeInMs() OVERRIDE {
    return max_cancel_to_down_time_in_ms_;
  }

  virtual int MaxTapGapTimeInMs() OVERRIDE {
    return max_tap_gap_time_in_ms_;
  }

  virtual void DropStashedTapDown() OVERRIDE {
    last_actions_ |= TAP_DOWN_DROPPED;
  }

  virtual void ForwardStashedTapDown() OVERRIDE {
    last_actions_ |= STASHED_TAP_DOWN_FORWARDED;
  }

  // Hiding some derived public methods
  using TapSuppressionController::GestureFlingCancel;
  using TapSuppressionController::GestureFlingCancelAck;
  using TapSuppressionController::ShouldDeferTapDown;
  using TapSuppressionController::ShouldSuppressTapEnd;

  int max_cancel_to_down_time_in_ms_;
  int max_tap_gap_time_in_ms_;

  int last_actions_;

  base::TimeTicks time_;
  bool timer_started_;
  base::TimeTicks timer_expiry_time_;

  DISALLOW_COPY_AND_ASSIGN(MockTapSuppressionController);
};

class TapSuppressionControllerTest : public testing::Test {
 public:
  TapSuppressionControllerTest() {
  }
  virtual ~TapSuppressionControllerTest() {
  }

 protected:
  // testing::Test
  virtual void SetUp() {
    tap_suppression_controller_.reset(new MockTapSuppressionController());
  }

  virtual void TearDown() {
    tap_suppression_controller_.reset();
  }

  scoped_ptr<MockTapSuppressionController> tap_suppression_controller_;
};

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown and everything happens without any delays.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapFast) {
  tap_suppression_controller_->set_max_cancel_to_down_time_in_ms(10);
  tap_suppression_controller_->set_max_tap_gap_time_in_ms(10);

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown, but there is a small delay between TapDown and TapUp.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapInsufficientlyLateTapUp) {
  tap_suppression_controller_->set_max_cancel_to_down_time_in_ms(10);
  tap_suppression_controller_->set_max_tap_gap_time_in_ms(10);

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Wait less than allowed delay between TapDown and TapUp, so they are still
  // considered a tap.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(7));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown, but there is a long delay between TapDown and TapUp.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapSufficientlyLateTapUp) {
  tap_suppression_controller_->set_max_cancel_to_down_time_in_ms(10);
  tap_suppression_controller_->set_max_tap_gap_time_in_ms(10);

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send processed GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send MouseDown. This MouseDown should be suppressed, for now.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Wait more than allowed delay between TapDown and TapUp, so they are not
  // considered a tap. This should release the previously suppressed TapDown.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(13));
  EXPECT_EQ(MockTapSuppressionController::STASHED_TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown, but there is a small delay between the Ack and TapDown.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapInsufficientlyLateTapDown) {
  tap_suppression_controller_->set_max_cancel_to_down_time_in_ms(10);
  tap_suppression_controller_->set_max_tap_gap_time_in_ms(10);

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Wait less than allowed delay between GestureFlingCancel and TapDown, so the
  // TapDown is still considered associated with the GestureFlingCancel.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(7));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown, but there is a long delay between the Ack and TapDown.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapSufficientlyLateTapDown) {
  tap_suppression_controller_->set_max_cancel_to_down_time_in_ms(10);
  tap_suppression_controller_->set_max_tap_gap_time_in_ms(10);

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Wait more than allowed delay between GestureFlingCancel and TapDown, so the
  // TapDown is not considered associated with the GestureFlingCancel.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(13));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should not be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());

  // Send MouseUp. This MouseUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when unprocessed GestureFlingCancel Ack
// comes after TapDown and everything happens without any delay.
TEST_F(TapSuppressionControllerTest, GFCAckUnprocessedAfterTapFast) {
  tap_suppression_controller_->set_max_cancel_to_down_time_in_ms(10);
  tap_suppression_controller_->set_max_tap_gap_time_in_ms(10);

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed, for now.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send unprocessed GestureFlingCancel Ack. This should release the
  // previously suppressed TapDown.
  tap_suppression_controller_->SendGestureFlingCancelAck(false);
  EXPECT_EQ(MockTapSuppressionController::STASHED_TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when processed GestureFlingCancel Ack comes
// after TapDown and everything happens without any delay.
TEST_F(TapSuppressionControllerTest, GFCAckProcessedAfterTapFast) {
  tap_suppression_controller_->set_max_cancel_to_down_time_in_ms(10);
  tap_suppression_controller_->set_max_tap_gap_time_in_ms(10);

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send processed GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes after
// TapDown and there is a small delay between the Ack and TapUp.
TEST_F(TapSuppressionControllerTest, GFCAckAfterTapInsufficientlyLateTapUp) {
  tap_suppression_controller_->set_max_cancel_to_down_time_in_ms(10);
  tap_suppression_controller_->set_max_tap_gap_time_in_ms(10);

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Wait less than allowed delay between TapDown and TapUp, so they are still
  // considered as a tap.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(7));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes after
// TapDown and there is a long delay between the Ack and TapUp.
TEST_F(TapSuppressionControllerTest, GFCAckAfterTapSufficientlyLateTapUp) {
  tap_suppression_controller_->set_max_cancel_to_down_time_in_ms(10);
  tap_suppression_controller_->set_max_tap_gap_time_in_ms(10);

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed, for now.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Wait more than allowed delay between TapDown and TapUp, so they are not
  // considered as a tap. This should release the previously suppressed TapDown.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(13));
  EXPECT_EQ(MockTapSuppressionController::STASHED_TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

}  // namespace content
