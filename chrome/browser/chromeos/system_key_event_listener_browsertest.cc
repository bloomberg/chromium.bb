// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <X11/keysym.h>

#include <cstring>

#include "base/message_loop.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/x/x11_util.h"

#include <X11/Xlib.h>  // should be here since it #defines lots of macros.

namespace chromeos {

class SystemKeyEventListenerTest : public InProcessBrowserTest {
 public:
  SystemKeyEventListenerTest()
      : manager_(input_method::InputMethodManager::GetInstance()),
        initial_caps_lock_state_(manager_->GetXKeyboard()->CapsLockIsEnabled()),
        num_lock_mask_(manager_->GetXKeyboard()->GetNumLockMask()),
        listener_(NULL) {
    CHECK(num_lock_mask_);
  }

 protected:
  class CapsLockObserver : public SystemKeyEventListener::CapsLockObserver {
   public:
    CapsLockObserver() {
    }

   private:
    virtual void OnCapsLockChange(bool enabled) {
      MessageLoopForUI::current()->Quit();
    }

    DISALLOW_COPY_AND_ASSIGN(CapsLockObserver);
  };

  // Start listening for X events.
  virtual void SetUpOnMainThread() OVERRIDE {
    SystemKeyEventListener::Initialize();
    listener_ = SystemKeyEventListener::GetInstance();
  }

  // Stop listening for X events.
  virtual void CleanUpOnMainThread() OVERRIDE {
    SystemKeyEventListener::Shutdown();
    listener_ = NULL;
    MessageLoopForUI::current()->RunAllPending();
  }

  // Feed a key event to SystemKeyEventListener. Returnes true if the event is
  // consumed by the listener.
  bool SendFakeEvent(KeySym keysym, int modifiers, bool is_press) {
    scoped_ptr<XEvent> event(SynthesizeKeyEvent(keysym, modifiers, is_press));
    // It seems that the Xvfb fake X server in build bots ignores a key event
    // synthesized by XTestFakeKeyEvent() or XSendEvent(). We just call
    // ProcessedXEvent() instead.
    return listener_->ProcessedXEvent(event.get());
  }

  input_method::InputMethodManager* manager_;
  const bool initial_caps_lock_state_;
  const unsigned int num_lock_mask_;
  SystemKeyEventListener* listener_;
  CapsLockObserver observer_;

 private:
  XEvent* SynthesizeKeyEvent(KeySym keysym, int modifiers, bool is_press) {
    XEvent* event = new XEvent;
    std::memset(event, 0, sizeof(XEvent));

    Display* display = ui::GetXDisplay();
    Window focused;
    int dummy;
    XGetInputFocus(display, &focused, &dummy);

    XKeyEvent* key_event = &event->xkey;
    key_event->display = display;
    key_event->keycode = XKeysymToKeycode(display, keysym);
    key_event->root = ui::GetX11RootWindow();
    key_event->same_screen = True;
    key_event->send_event = False;
    key_event->state = modifiers;
    key_event->subwindow = None;
    key_event->time = CurrentTime;
    key_event->type = is_press ? KeyPress : KeyRelease;
    key_event->window = focused;
    key_event->x = key_event->x_root = key_event->y = key_event->y_root = 1;

    return event;
  }

  DISALLOW_COPY_AND_ASSIGN(SystemKeyEventListenerTest);
};

// Tests if the current Caps Lock status is toggled and OnCapsLockChange method
// is called when both Shift keys are pressed.
IN_PROC_BROWSER_TEST_F(SystemKeyEventListenerTest, TestCapsLock) {
  listener_->AddCapsLockObserver(&observer_);

  // Press both Shift keys. Note that ProcessedXEvent() returns false even when
  // the second Shift key is pressed.
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, 0, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask, false));
  // Enter a new message loop and wait for OnCapsLockChange() to be called.
  ui_test_utils::RunMessageLoop();
  // Make sure the Caps Lock status is actually changed.
  EXPECT_EQ(
      !initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  // Test all other Shift_L/R combinations.
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, 0, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask, false));
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(
      initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, 0, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask, false));
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(
      !initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, 0, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask, false));
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(
      initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  // Restore the original state.
  listener_->RemoveCapsLockObserver(&observer_);
  manager_->GetXKeyboard()->SetCapsLockEnabled(initial_caps_lock_state_);
}

// Tests the same above, but with Num Lock. Pressing both Shift keys should
// toggle Caps Lock even when Num Lock is enabled. See crosbug.com/23067.
IN_PROC_BROWSER_TEST_F(SystemKeyEventListenerTest, TestCapsLockWithNumLock) {
  listener_->AddCapsLockObserver(&observer_);

  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, num_lock_mask_, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | num_lock_mask_, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | num_lock_mask_, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask | num_lock_mask_, false));
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(
      !initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, num_lock_mask_, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | num_lock_mask_, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask | num_lock_mask_, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | num_lock_mask_, false));
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(
      initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, num_lock_mask_, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask | num_lock_mask_, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask | num_lock_mask_, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | num_lock_mask_, false));
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(
      !initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, num_lock_mask_, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask | num_lock_mask_, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | num_lock_mask_, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask | num_lock_mask_, false));
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(
      initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  listener_->RemoveCapsLockObserver(&observer_);
  manager_->GetXKeyboard()->SetCapsLockEnabled(initial_caps_lock_state_);
}

// Test pressing Shift_L+R with an another modifier like Control. Caps Lock
// status should not be changed this time.
IN_PROC_BROWSER_TEST_F(SystemKeyEventListenerTest, TestCapsLockWithControl) {
  listener_->AddCapsLockObserver(&observer_);

  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ControlMask, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | ControlMask, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | ControlMask, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask | ControlMask, false));
  EXPECT_EQ(
      initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  listener_->RemoveCapsLockObserver(&observer_);
  manager_->GetXKeyboard()->SetCapsLockEnabled(initial_caps_lock_state_);
}

// TODO(yusukes): Test IME hot keys.

}  // namespace chromeos
