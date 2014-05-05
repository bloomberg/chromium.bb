// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/touch_exploration_controller.h"

#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/test/test_event_handler.h"

namespace ui {

class TouchExplorationTest : public InProcessBrowserTest {
 public:
  TouchExplorationTest() {}
  virtual ~TouchExplorationTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(ash::switches::kAshEnableTouchExplorationMode);
  }

 protected:
  void SwitchTouchExplorationMode(bool on) {
    ash::AccessibilityDelegate* ad =
        ash::Shell::GetInstance()->accessibility_delegate();
    if (on != ad->IsSpokenFeedbackEnabled())
      ad->ToggleSpokenFeedback(ash::A11Y_NOTIFICATION_NONE);
  }

private:
  DISALLOW_COPY_AND_ASSIGN(TouchExplorationTest);
};

IN_PROC_BROWSER_TEST_F(TouchExplorationTest, PRE_ToggleOnOff) {
  // TODO (mfomitchev): If the test is run by itself, there is a resize at the
  //  very beginning. An in-progress resize creates a "resize lock" in
  // RenderWidgetHostViewAura, which calls
  // WindowEventDispatcher::HoldPointerMoves(), which prevents mouse events from
  // coming through. Adding a PRE_ test ensures the resize completes before the
  // actual test is executed. sadrul@ says the resize shouldn't be even
  // happening, so this needs to be looked at further.
}

// This test turns the touch exploration mode on/off and confirms that events
// get rewritten when the touch exploration mode is on, and aren't affected
// after the touch exploration mode is turned off.
IN_PROC_BROWSER_TEST_F(TouchExplorationTest, ToggleOnOff) {
  aura::Window* root_window = ash::Shell::GetInstance()->GetPrimaryRootWindow();
  scoped_ptr<ui::test::TestEventHandler>
      event_handler(new ui::test::TestEventHandler());
  root_window->AddPreTargetHandler(event_handler.get());
  SwitchTouchExplorationMode(true);
  aura::test::EventGenerator generator(root_window);

  generator.PressTouchId(1);
  // Number of mouse events may be greater than 1 because of ET_MOUSE_ENTERED.
  EXPECT_GT(event_handler->num_mouse_events(), 0);
  EXPECT_EQ(0, event_handler->num_touch_events());
  event_handler->Reset();

  SwitchTouchExplorationMode(false);
  generator.MoveTouchId(gfx::Point(11, 12), 1);
  EXPECT_EQ(0, event_handler->num_mouse_events());
  EXPECT_EQ(1, event_handler->num_touch_events());
  event_handler->Reset();

  SwitchTouchExplorationMode(true);
  generator.PressTouchId(2);
  EXPECT_GT(event_handler->num_mouse_events(), 0);
  EXPECT_EQ(0, event_handler->num_touch_events());

  SwitchTouchExplorationMode(false);
  root_window->RemovePreTargetHandler(event_handler.get());
}

}  // namespace ui
