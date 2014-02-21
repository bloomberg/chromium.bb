// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

class MouseEventCapturer : public ui::EventHandler {
 public:
  MouseEventCapturer() { Reset(); }
  virtual ~MouseEventCapturer() {}

  void Reset() {
    events_.clear();
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (!(event->flags() & ui::EF_LEFT_MOUSE_BUTTON))
      return;
    // Filter out extraneous mouse events like mouse entered, exited,
    // capture changed, etc.
    ui::EventType type = event->type();
    if (type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_PRESSED ||
        type == ui::ET_MOUSE_RELEASED) {
      events_.push_back(ui::MouseEvent(
          event->type(),
          event->location(),
          event->root_location(),
          event->flags(),
          event->changed_button_flags()));
      // Stop event propagation so we don't click on random stuff that
      // might break test assumptions.
      event->StopPropagation();
    }

    // If there is a possibility that we're in an infinite loop, we should
    // exit early with a sensible error rather than letting the test time out.
    ASSERT_LT(events_.size(), 100u);
  }

  const std::vector<ui::MouseEvent>& captured_events() const {
    return events_;
  }

 private:
  std::vector<ui::MouseEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventCapturer);
};

class AutoclickTest : public test::AshTestBase {
 public:
  AutoclickTest() {}
  virtual ~AutoclickTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    Shell::GetInstance()->AddPreTargetHandler(&mouse_event_capturer_);
    GetAutoclickController()->SetAutoclickDelay(0);

    // Move mouse to deterministic location at the start of each test.
    GetEventGenerator().MoveMouseTo(100, 100);
  }

  virtual void TearDown() OVERRIDE {
    Shell::GetInstance()->RemovePreTargetHandler(&mouse_event_capturer_);
    test::AshTestBase::TearDown();
  }

  void MoveMouseWithFlagsTo(int x, int y, ui::EventFlags flags) {
    GetEventGenerator().set_flags(flags);
    GetEventGenerator().MoveMouseTo(x, y);
    GetEventGenerator().set_flags(ui::EF_NONE);
  }

  const std::vector<ui::MouseEvent>& WaitForMouseEvents() {
    mouse_event_capturer_.Reset();
    RunAllPendingInMessageLoop();
    return mouse_event_capturer_.captured_events();
  }

  AutoclickController* GetAutoclickController() {
    return Shell::GetInstance()->autoclick_controller();
  }

 private:
  MouseEventCapturer mouse_event_capturer_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickTest);
};

TEST_F(AutoclickTest, ToggleEnabled) {
  std::vector<ui::MouseEvent> events;

  // We should not see any events initially.
  EXPECT_FALSE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Enable autoclick, and we should see a mouse pressed and
  // a mouse released event, simulating a click.
  GetAutoclickController()->SetEnabled(true);
  GetEventGenerator().MoveMouseTo(0, 0);
  EXPECT_TRUE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[1].type());

  // We should not get any more clicks until we move the mouse.
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
  GetEventGenerator().MoveMouseTo(30, 30);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[1].type());

  // Disable autoclick, and we should see the original behaviour.
  GetAutoclickController()->SetEnabled(false);
  EXPECT_FALSE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
}

TEST_F(AutoclickTest, MouseMovement) {
  std::vector<ui::MouseEvent> events;
  GetAutoclickController()->SetEnabled(true);

  gfx::Point p1(0, 0);
  gfx::Point p2(20, 20);
  gfx::Point p3(40, 40);

  // Move mouse to p1.
  GetEventGenerator().MoveMouseTo(p1);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(p1.ToString(), events[0].root_location().ToString());
  EXPECT_EQ(p1.ToString(), events[1].root_location().ToString());

  // Move mouse to multiple locations and finally arrive at p3.
  GetEventGenerator().MoveMouseTo(p2);
  GetEventGenerator().MoveMouseTo(p1);
  GetEventGenerator().MoveMouseTo(p3);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(p3.ToString(), events[0].root_location().ToString());
  EXPECT_EQ(p3.ToString(), events[1].root_location().ToString());
}

TEST_F(AutoclickTest, MovementThreshold) {
  GetAutoclickController()->SetEnabled(true);
  GetEventGenerator().MoveMouseTo(0, 0);
  EXPECT_EQ(2u, WaitForMouseEvents().size());

  // Small mouse movements should not trigger an autoclick.
  GetEventGenerator().MoveMouseTo(1, 1);
  EXPECT_EQ(0u, WaitForMouseEvents().size());
  GetEventGenerator().MoveMouseTo(2, 2);
  EXPECT_EQ(0u, WaitForMouseEvents().size());
  GetEventGenerator().MoveMouseTo(0, 0);
  EXPECT_EQ(0u, WaitForMouseEvents().size());

  // A large mouse movement should trigger an autoclick.
  GetEventGenerator().MoveMouseTo(100, 100);
  EXPECT_EQ(2u, WaitForMouseEvents().size());
}

TEST_F(AutoclickTest, SingleKeyModifier) {
  GetAutoclickController()->SetEnabled(true);
  MoveMouseWithFlagsTo(20, 20, ui::EF_SHIFT_DOWN);
  std::vector<ui::MouseEvent> events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, events[0].flags() & ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ui::EF_SHIFT_DOWN, events[1].flags() & ui::EF_SHIFT_DOWN);
}

TEST_F(AutoclickTest, MultipleKeyModifiers) {
  GetAutoclickController()->SetEnabled(true);
  ui::EventFlags modifier_flags = static_cast<ui::EventFlags>(
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  MoveMouseWithFlagsTo(30, 30, modifier_flags);
  std::vector<ui::MouseEvent> events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(modifier_flags, events[0].flags() & modifier_flags);
  EXPECT_EQ(modifier_flags, events[1].flags() & modifier_flags);
}

TEST_F(AutoclickTest, KeyModifiersReleased) {
  GetAutoclickController()->SetEnabled(true);

  ui::EventFlags modifier_flags = static_cast<ui::EventFlags>(
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  MoveMouseWithFlagsTo(12, 12, modifier_flags);

  // Simulate releasing key modifiers by sending key released events.
  GetEventGenerator().ReleaseKey(ui::VKEY_CONTROL,
      static_cast<ui::EventFlags>(ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN));
  GetEventGenerator().ReleaseKey(ui::VKEY_SHIFT, ui::EF_ALT_DOWN);

  std::vector<ui::MouseEvent> events;
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(0, events[0].flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(0, events[0].flags() & ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ui::EF_ALT_DOWN, events[0].flags() & ui::EF_ALT_DOWN);
}

TEST_F(AutoclickTest, ExtendedDisplay) {
  UpdateDisplay("1280x1024,800x600");
  RunAllPendingInMessageLoop();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2u, root_windows.size());

  GetAutoclickController()->SetEnabled(true);
  std::vector<ui::MouseEvent> events;

  // Test first root window.
  aura::test::EventGenerator generator1(root_windows[0]);
  generator1.MoveMouseTo(100, 200);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(100, events[0].root_location().x());
  EXPECT_EQ(200, events[0].root_location().y());

  // Test second root window.
  aura::test::EventGenerator generator2(root_windows[1]);
  generator2.MoveMouseTo(300, 400);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(300, events[0].root_location().x());
  EXPECT_EQ(400, events[0].root_location().y());

  // Test movement threshold between displays.
}

TEST_F(AutoclickTest, UserInputCancelsAutoclick) {
  GetAutoclickController()->SetEnabled(true);
  std::vector<ui::MouseEvent> events;

  // Pressing a normal key should cancel the autoclick.
  GetEventGenerator().MoveMouseTo(200, 200);
  GetEventGenerator().PressKey(ui::VKEY_K, ui::EF_NONE);
  GetEventGenerator().ReleaseKey(ui::VKEY_K, ui::EF_NONE);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Pressing a modifier key should NOT cancel the autoclick.
  GetEventGenerator().MoveMouseTo(100, 100);
  GetEventGenerator().PressKey(ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN);
  GetEventGenerator().ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());

  // Performing a gesture should cancel the autoclick.
  GetEventGenerator().MoveMouseTo(200, 200);
  GetEventGenerator().GestureTapDownAndUp(gfx::Point(100, 100));
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Test another gesture.
  GetEventGenerator().MoveMouseTo(100, 100);
  GetEventGenerator().GestureScrollSequence(
      gfx::Point(100, 100),
      gfx::Point(200, 200),
      base::TimeDelta::FromMilliseconds(200),
      3);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Test scroll events.
  GetEventGenerator().MoveMouseTo(200, 200);
  GetEventGenerator().ScrollSequence(
      gfx::Point(100, 100), base::TimeDelta::FromMilliseconds(200),
      0, 100, 3, 2);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
}

TEST_F(AutoclickTest, SynthesizedMouseMovesIgnored) {
  GetAutoclickController()->SetEnabled(true);
  std::vector<ui::MouseEvent> events;
  GetEventGenerator().MoveMouseTo(100, 100);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());

  // Show a window and make sure the new window is under the cursor. As a
  // result, synthesized mouse events will be dispatched to the window, but it
  // should not trigger an autoclick.
  aura::test::EventCountDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, 123, gfx::Rect(50, 50, 100, 100)));
  window->Show();
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
  EXPECT_EQ("1 1 0", delegate.GetMouseMotionCountsAndReset());
}

}  // namespace ash
