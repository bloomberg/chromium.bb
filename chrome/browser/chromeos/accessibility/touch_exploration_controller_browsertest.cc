// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/touch_exploration_controller.h"

#include "ash/accessibility_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
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

// This test turns the touch exploration mode on/off and confirms that events
// get rewritten when the touch exploration mode is on, and aren't affected
// after the touch exploration mode is turned off.
IN_PROC_BROWSER_TEST_F(TouchExplorationTest, ToggleOnOff) {
  // The RenderView for WebContents is created as a result of the navigation
  // to the New Tab page which is done as part of the test SetUp. The creation
  // involves sending a resize message to the renderer process. Here we wait
  // for the resize ack to be received, because currently WindowEventDispatcher
  // has code to hold touch and mouse move events until resize is complete
  // (crbug.com/384342) which interferes with this test.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForResizeComplete(web_contents);
  aura::Window* root_window = ash::Shell::GetInstance()->GetPrimaryRootWindow();
  scoped_ptr<ui::test::TestEventHandler>
      event_handler(new ui::test::TestEventHandler());
  root_window->AddPreTargetHandler(event_handler.get());
  SwitchTouchExplorationMode(true);
  aura::test::EventGenerator generator(root_window);

  generator.set_current_location(gfx::Point(100, 200));
  generator.PressTouchId(1);
  // Since the touch exploration controller doesn't know if the user is
  // double-tapping or not, touch exploration is only initiated if the
  // user moves more than 8 pixels away from the initial location (the "slop"),
  // or after 300 ms has elapsed.
  generator.MoveTouchId(gfx::Point(109, 209), 1);
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
  generator.set_current_location(gfx::Point(500, 600));
  generator.PressTouchId(2);
  generator.MoveTouchId(gfx::Point(509, 609), 2);
  EXPECT_GT(event_handler->num_mouse_events(), 0);
  EXPECT_EQ(0, event_handler->num_touch_events());

  SwitchTouchExplorationMode(false);
  root_window->RemovePreTargetHandler(event_handler.get());
}

}  // namespace ui
