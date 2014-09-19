// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/home_card_gesture_manager.h"

#include "athena/home/home_card_constants.h"
#include "athena/home/public/home_card.h"
#include "athena/test/athena_test_base.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace athena {

class HomeCardGestureManagerTest : public test::AthenaTestBase,
                                   public HomeCardGestureManager::Delegate {
 public:
  HomeCardGestureManagerTest()
      : final_state_(HomeCard::HIDDEN),
        last_from_state_(HomeCard::HIDDEN),
        last_to_state_(HomeCard::HIDDEN),
        last_progress_(0.0f),
        was_fling_(false),
        last_y_(0),
        progress_count_(0),
        end_count_(0) {}
  virtual ~HomeCardGestureManagerTest() {}

  // testing::Test:
  virtual void SetUp() OVERRIDE {
    test::AthenaTestBase::SetUp();
    gesture_manager_.reset(new HomeCardGestureManager(this, screen_bounds()));
  }

 protected:
  int GetEndCountAndReset() {
    int result = end_count_;
    end_count_ = 0;
    return result;
  }
  int GetProgressCountAndReset() {
    int result = progress_count_;
    progress_count_ = 0;
    return result;
  }

  // Process a gesture event for our use case.
  bool ProcessGestureEvent(ui::EventType type, int y) {
    ui::GestureEventDetails details;
    if (type == ui::ET_GESTURE_SCROLL_BEGIN ||
        type == ui::ET_GESTURE_SCROLL_UPDATE)
      details = ui::GestureEventDetails(type, 0, (y - last_y_));
    else
      details = ui::GestureEventDetails(type);
    ui::GestureEvent event(0, y, ui::EF_NONE, base::TimeDelta(), details);
    if (type == ui::ET_GESTURE_SCROLL_BEGIN) {
      // Compute the position that the home card would have wrt to the top of
      // the screen if the screen had screen_bounds().
      HomeCard::State state = HomeCard::Get()->GetState();
      int home_card_top = 0;
      if (state == HomeCard::VISIBLE_BOTTOM)
        home_card_top = screen_bounds().height() - kHomeCardHeight;
      else if (state == HomeCard::VISIBLE_MINIMIZED)
        home_card_top = screen_bounds().height() - kHomeCardMinimizedHeight;

      gfx::Point location = event.location();
      location.set_y(location.y() - home_card_top);
      event.set_location(location);
    }
    gesture_manager_->ProcessGestureEvent(&event);
    last_y_ = y;
    return event.handled();
  }

  void ProcessFlingGesture(float velocity) {
    ui::GestureEvent event(0, last_y_, ui::EF_NONE, base::TimeDelta(),
                           ui::GestureEventDetails(
                               ui::ET_SCROLL_FLING_START, 0, velocity));
    gesture_manager_->ProcessGestureEvent(&event);
  }

  int screen_height() const {
    return screen_bounds().height();
  }

  HomeCard::State final_state_;
  HomeCard::State last_from_state_;
  HomeCard::State last_to_state_;
  float last_progress_;
  bool was_fling_;

 private:
  gfx::Rect screen_bounds() const {
    return gfx::Rect(0, 0, 1280, 1024);
  }

  // HomeCardGestureManager::Delegate:
  virtual void OnGestureEnded(HomeCard::State final_state,
                              bool is_fling) OVERRIDE {
    final_state_ = final_state;
    was_fling_ = is_fling;
    ++end_count_;
  }

  virtual void OnGestureProgressed(HomeCard::State from_state,
                                   HomeCard::State to_state,
                                   float progress) OVERRIDE {
    last_from_state_ = from_state;
    last_to_state_ = to_state;
    last_progress_ = progress;
    ++progress_count_;
  }

  int last_y_;
  int progress_count_;
  int end_count_;
  scoped_ptr<HomeCardGestureManager> gesture_manager_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardGestureManagerTest);
};

TEST_F(HomeCardGestureManagerTest, Basic) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 1020));
  EXPECT_EQ(0, GetEndCountAndReset());
  EXPECT_EQ(0, GetProgressCountAndReset());

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1019);
  EXPECT_EQ(1, GetProgressCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, last_from_state_);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, last_to_state_);
  EXPECT_GT(1.0f, last_progress_);

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1020);
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1022);
  EXPECT_EQ(2, GetProgressCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, last_from_state_);
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, last_to_state_);
  EXPECT_EQ(1.0f, last_progress_);

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1010);
  float progress_1010 = last_progress_;
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1008);
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1000);
  EXPECT_EQ(3, GetProgressCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, last_from_state_);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, last_to_state_);
  EXPECT_LT(progress_1010, last_progress_);

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 900);
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 800);
  EXPECT_EQ(2, GetProgressCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, last_from_state_);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, last_to_state_);
  float progress_800 = last_progress_;
  EXPECT_GT(1.0f, last_progress_);
  EXPECT_LT(0.0f, last_progress_);

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 790);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, last_from_state_);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, last_to_state_);
  EXPECT_LT(progress_800, last_progress_);

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 810);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, last_from_state_);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, last_to_state_);
  EXPECT_GT(progress_800, last_progress_);

  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_END, 810));
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, final_state_);
  EXPECT_FALSE(was_fling_);
}

// Test gesture progress when the gesture is initiated when the home card is in
// the centered state.
TEST_F(HomeCardGestureManagerTest, StartCentered) {
  HomeCard::Get()->SetState(HomeCard::VISIBLE_CENTERED);

  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 20));

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 10);
  EXPECT_EQ(1, GetProgressCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, last_from_state_);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, last_to_state_);
  EXPECT_EQ(1.0f, last_progress_);

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 900);
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 910);
  EXPECT_EQ(2, GetProgressCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, last_from_state_);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, last_to_state_);
  EXPECT_GT(1.0f, last_progress_);
  EXPECT_LT(0.0f, last_progress_);

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 960);
  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_END, 960));
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, final_state_);
}

// Test gesture progress when the gesture is initiated when the home card is in
// the centered state.
TEST_F(HomeCardGestureManagerTest, StartBottom) {
  HomeCard::Get()->SetState(HomeCard::VISIBLE_BOTTOM);

  // No changes for slight moves.
  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 950));
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 960);
  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_END, 960));
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, final_state_);

  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 950));
  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_END, 800));
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, final_state_);

  // State change for the bigger moves.
  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 950));
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1000);
  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_END, 1000));
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, final_state_);

  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 950));
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 300);
  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_END, 300));
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, final_state_);
  EXPECT_FALSE(was_fling_);
}

TEST_F(HomeCardGestureManagerTest, FlingUpAtEnd) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 1020));
  EXPECT_EQ(0, GetEndCountAndReset());
  EXPECT_EQ(0, GetProgressCountAndReset());

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1010);
  ProcessFlingGesture(-150.0f);
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, final_state_);
  EXPECT_TRUE(was_fling_);
}

TEST_F(HomeCardGestureManagerTest, FlingDownAtEnd) {
  HomeCard::Get()->SetState(HomeCard::VISIBLE_CENTERED);

  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 20));
  EXPECT_EQ(0, GetEndCountAndReset());
  EXPECT_EQ(0, GetProgressCountAndReset());

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 30);
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 100);
  ProcessFlingGesture(150.0f);
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, final_state_);
  EXPECT_TRUE(was_fling_);
}

TEST_F(HomeCardGestureManagerTest, WeakFling) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 1020));
  EXPECT_EQ(0, GetEndCountAndReset());
  EXPECT_EQ(0, GetProgressCountAndReset());

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1010);
  ProcessFlingGesture(-30.0f);
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, final_state_);
  EXPECT_FALSE(was_fling_);
}

// Test the situation where the user intends a single fling but the finger
// touches the screen long enough, so that the home card becomes bigger than the
// height of VISIBLE_BOTTOM state due to the scroll events.
// In this case the fling event should not change the final state from
// VISIBLE_BOTTOM to VISIBLE_CENTERED because the user's intention was a single
// fling. See http://crbug.com/415211
TEST_F(HomeCardGestureManagerTest, FastFling) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  EXPECT_TRUE(ProcessGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, 1020));
  EXPECT_EQ(0, GetEndCountAndReset());
  EXPECT_EQ(0, GetProgressCountAndReset());

  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE, 1010);
  ProcessGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE,
                      screen_height() - kHomeCardHeight);
  ProcessFlingGesture(-150.0f);
  EXPECT_EQ(1, GetEndCountAndReset());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, final_state_);
  EXPECT_TRUE(was_fling_);
}

}  // namespace athena
