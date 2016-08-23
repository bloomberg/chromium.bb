// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/widget/widget.h"

namespace ash {

using PointerWatcherAdapterTest = test::AshTestBase;

// Records calls to OnPointerEventObserved() in |pointer_event_count_| and
// calls to OnMouseCaptureChanged() to |capture_changed_count_|.
class TestPointerWatcher : public views::PointerWatcher {
 public:
  explicit TestPointerWatcher(bool wants_moves) {
    WmShell::Get()->AddPointerWatcher(this, wants_moves);
  }
  ~TestPointerWatcher() override { WmShell::Get()->RemovePointerWatcher(this); }

  void ClearCounts() { pointer_event_count_ = capture_changed_count_ = 0; }

  int pointer_event_count() const { return pointer_event_count_; }
  int capture_changed_count() const { return capture_changed_count_; }

  // views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              views::Widget* target) override {
    pointer_event_count_++;
  }
  void OnMouseCaptureChanged() override { capture_changed_count_++; }

 private:
  int pointer_event_count_ = 0;
  int capture_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestPointerWatcher);
};

// Creates two TestPointerWatchers, one that wants moves and one that doesn't.
class TestHelper {
 public:
  TestHelper() : non_move_watcher_(false), move_watcher_(true) {}
  ~TestHelper() {}

  // Used to verify call counts.
  void ExpectCallCount(int non_move_pointer_event_count,
                       int non_move_capture_changed_count,
                       int move_pointer_event_count,
                       int move_capture_changed_count) {
    EXPECT_EQ(non_move_pointer_event_count,
              non_move_watcher_.pointer_event_count());
    EXPECT_EQ(non_move_capture_changed_count,
              non_move_watcher_.capture_changed_count());
    EXPECT_EQ(move_pointer_event_count, move_watcher_.pointer_event_count());
    EXPECT_EQ(move_capture_changed_count,
              move_watcher_.capture_changed_count());

    non_move_watcher_.ClearCounts();
    move_watcher_.ClearCounts();
  }

 private:
  TestPointerWatcher non_move_watcher_;
  TestPointerWatcher move_watcher_;

  DISALLOW_COPY_AND_ASSIGN(TestHelper);
};

TEST_F(PointerWatcherAdapterTest, MouseEvents) {
  TestHelper helper;

  // Move: only the move PointerWatcher should get the event.
  GetEventGenerator().MoveMouseTo(gfx::Point(10, 10));
  helper.ExpectCallCount(0, 0, 1, 0);

  // Press: both.
  GetEventGenerator().PressLeftButton();
  helper.ExpectCallCount(1, 0, 1, 0);

  // Drag: none.
  GetEventGenerator().MoveMouseTo(gfx::Point(20, 30));
  helper.ExpectCallCount(0, 0, 0, 0);

  // Release: both (aura generates a capture event here).
  GetEventGenerator().ReleaseLeftButton();
  helper.ExpectCallCount(1, 1, 1, 1);

  // Exit: none.
  GetEventGenerator().SendMouseExit();
  helper.ExpectCallCount(0, 0, 0, 0);

  // Enter: none.
  ui::MouseEvent enter_event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  GetEventGenerator().Dispatch(&enter_event);
  helper.ExpectCallCount(0, 0, 0, 0);

  // Wheel: none
  GetEventGenerator().MoveMouseWheel(10, 11);
  helper.ExpectCallCount(0, 0, 0, 0);

  // Capture: both.
  ui::MouseEvent capture_event(ui::ET_MOUSE_CAPTURE_CHANGED, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(), 0, 0);
  GetEventGenerator().Dispatch(&capture_event);
  helper.ExpectCallCount(0, 1, 0, 1);
}

TEST_F(PointerWatcherAdapterTest, TouchEvents) {
  TestHelper helper;

  // Press: both.
  const int touch_id = 11;
  GetEventGenerator().PressTouchId(touch_id);
  helper.ExpectCallCount(1, 0, 1, 0);

  // Drag: none.
  GetEventGenerator().MoveTouchId(gfx::Point(20, 30), touch_id);
  helper.ExpectCallCount(0, 0, 0, 0);

  // Release: both (contrary to mouse above, touch does not implicitly generate
  // capture).
  GetEventGenerator().ReleaseTouchId(touch_id);
  helper.ExpectCallCount(1, 0, 1, 0);
}

}  // namespace ash
