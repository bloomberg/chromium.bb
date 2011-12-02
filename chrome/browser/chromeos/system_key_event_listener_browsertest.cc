// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <X11/keysym.h>
#include <X11/Xlib.h>
#undef Bool
#undef None
#undef Status

#include <cstring>

#include "base/message_loop.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/x/x11_util.h"

#if defined(USE_VIRTUAL_KEYBOARD)
// Since USE_VIRTUAL_KEYBOARD build only supports a few keyboard layouts, we
// skip the tests for now.
#define TestInputMethod DISABLED_TestInputMethod
#define TestInputMethodWithNumLock DISABLED_TestInputMethodWithNumLock
#define TestKoreanInputMethod DISABLED_TestKoreanInputMethod
#endif

namespace chromeos {
namespace {

static const char* active_input_methods[] = {
  "xkb:us::eng",  // The first one should be US Qwerty.
#if !defined(USE_VIRTUAL_KEYBOARD)
  "xkb:us:dvorak:eng",
  "xkb:kr:kr104:kor",  // for testing XK_space with ShiftMask.
  // TODO(yusukes): Add "mozc-jp", "xkb:jp::jpn", and "mozc-hangul" to test
  // more IME hot keys once build bots start supporting ibus-daemon.
#endif
};

class CapsLockObserver : public SystemKeyEventListener::CapsLockObserver {
 public:
  CapsLockObserver() {
  }

 private:
  virtual void OnCapsLockChange(bool enabled) OVERRIDE {
    MessageLoopForUI::current()->Quit();
  }

  DISALLOW_COPY_AND_ASSIGN(CapsLockObserver);
};

class InputMethodObserver : public input_method::InputMethodManager::Observer {
 public:
  InputMethodObserver() {
  }

  const std::string& current_input_method_id() const {
    return current_input_method_id_;
  }

 private:
  virtual void InputMethodChanged(
      input_method::InputMethodManager* manager,
      const input_method::InputMethodDescriptor& current_input_method,
      size_t num_active_input_methods) OVERRIDE {
    current_input_method_id_ = current_input_method.id();
  }
  virtual void ActiveInputMethodsChanged(
      input_method::InputMethodManager* manager,
      const input_method::InputMethodDescriptor& current_input_method,
      size_t num_active_input_methods) OVERRIDE {
  }
  virtual void PropertyListChanged(
      input_method::InputMethodManager* manager,
      const input_method::ImePropertyList& current_ime_properties) OVERRIDE {
  }

  std::string current_input_method_id_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodObserver);
};

}  // namespace

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
  // Start listening for X events.
  virtual void SetUpOnMainThread() OVERRIDE {
    ActivateInputMethods();
    SystemKeyEventListener::Initialize();
    listener_ = SystemKeyEventListener::GetInstance();
  }

  // Stop listening for X events.
  virtual void CleanUpOnMainThread() OVERRIDE {
    SystemKeyEventListener::Shutdown();
    listener_ = NULL;
    DeactivateInputMethods();
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

  // Enables input methods in active_input_methods[].
  void ActivateInputMethods() {
    input_method::ImeConfigValue value;
    value.type = input_method::ImeConfigValue::kValueTypeStringList;
    for (size_t i = 0; i < arraysize(active_input_methods); ++i)
      value.string_list_value.push_back(active_input_methods[i]);
    manager_->SetImeConfig(language_prefs::kGeneralSectionName,
                           language_prefs::kPreloadEnginesConfigName,
                           value);
  }

  // Disables all input methods except US Qwerty.
  void DeactivateInputMethods() {
    input_method::ImeConfigValue value;
    value.type = input_method::ImeConfigValue::kValueTypeStringList;
    value.string_list_value.push_back(active_input_methods[0]);  // Qwerty
    manager_->SetImeConfig(language_prefs::kGeneralSectionName,
                           language_prefs::kPreloadEnginesConfigName,
                           value);
  }

  input_method::InputMethodManager* manager_;
  const bool initial_caps_lock_state_;
  const unsigned int num_lock_mask_;
  SystemKeyEventListener* listener_;

  CapsLockObserver caps_lock_observer_;
  InputMethodObserver input_method_observer_;

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
    key_event->subwindow = 0L;  // None
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
  listener_->AddCapsLockObserver(&caps_lock_observer_);

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
  listener_->RemoveCapsLockObserver(&caps_lock_observer_);
  manager_->GetXKeyboard()->SetCapsLockEnabled(initial_caps_lock_state_);
}

// Tests the same above, but with Num Lock. Pressing both Shift keys should
// toggle Caps Lock even when Num Lock is enabled. See crosbug.com/23067.
IN_PROC_BROWSER_TEST_F(SystemKeyEventListenerTest, TestCapsLockWithNumLock) {
  listener_->AddCapsLockObserver(&caps_lock_observer_);

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

  listener_->RemoveCapsLockObserver(&caps_lock_observer_);
  manager_->GetXKeyboard()->SetCapsLockEnabled(initial_caps_lock_state_);
}

// Tests pressing Shift_L+R with an another modifier like Control. Caps Lock
// status should not be changed this time.
IN_PROC_BROWSER_TEST_F(SystemKeyEventListenerTest, TestCapsLockWithControl) {
  listener_->AddCapsLockObserver(&caps_lock_observer_);

  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ControlMask, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | ControlMask, true));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, ShiftMask | ControlMask, false));
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, ShiftMask | ControlMask, false));
  EXPECT_EQ(
      initial_caps_lock_state_, manager_->GetXKeyboard()->CapsLockIsEnabled());

  listener_->RemoveCapsLockObserver(&caps_lock_observer_);
  manager_->GetXKeyboard()->SetCapsLockEnabled(initial_caps_lock_state_);
}

// Tests IME hot keys.
IN_PROC_BROWSER_TEST_F(SystemKeyEventListenerTest, TestInputMethod) {
  manager_->AddObserver(&input_method_observer_);

  // Press Shift+Alt to select the second IME.
  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, 0, true));
  EXPECT_FALSE(SendFakeEvent(XK_Meta_L, ShiftMask, true));
  EXPECT_TRUE(SendFakeEvent(XK_Meta_L, ShiftMask | Mod1Mask, false));
  EXPECT_TRUE(SendFakeEvent(XK_Shift_L, ShiftMask, false));
  // Do not call ui_test_utils::RunMessageLoop() here since the observer gets
  // notified before returning from the SendFakeEvent call.
  EXPECT_TRUE(input_method_observer_.current_input_method_id() ==
              active_input_methods[1]);

  // Press Control+space to move back to the previous one.
  EXPECT_FALSE(SendFakeEvent(XK_Control_L, 0, true));
  EXPECT_TRUE(SendFakeEvent(XK_space, ControlMask, true));
  EXPECT_TRUE(SendFakeEvent(XK_space, ControlMask, false));
  EXPECT_TRUE(SendFakeEvent(XK_Control_L, ControlMask, false));
  EXPECT_TRUE(input_method_observer_.current_input_method_id() ==
              active_input_methods[0]);

  manager_->RemoveObserver(&input_method_observer_);
}

// Tests IME hot keys with Num Lock. See crosbug.com/23067.
IN_PROC_BROWSER_TEST_F(SystemKeyEventListenerTest, TestInputMethodWithNumLock) {
  manager_->AddObserver(&input_method_observer_);

  EXPECT_FALSE(SendFakeEvent(XK_Shift_L, num_lock_mask_, true));
  EXPECT_FALSE(SendFakeEvent(XK_Meta_L, ShiftMask | num_lock_mask_, true));
  EXPECT_TRUE(
      SendFakeEvent(XK_Meta_L, ShiftMask | Mod1Mask | num_lock_mask_, false));
  EXPECT_TRUE(SendFakeEvent(XK_Shift_L, ShiftMask | num_lock_mask_, false));
  EXPECT_TRUE(input_method_observer_.current_input_method_id() ==
              active_input_methods[1]);

  EXPECT_FALSE(SendFakeEvent(XK_Control_L, num_lock_mask_, true));
  EXPECT_TRUE(SendFakeEvent(XK_space, ControlMask | num_lock_mask_, true));
  EXPECT_TRUE(SendFakeEvent(XK_space, ControlMask | num_lock_mask_, false));
  EXPECT_TRUE(SendFakeEvent(XK_Control_L, ControlMask | num_lock_mask_, false));
  EXPECT_TRUE(input_method_observer_.current_input_method_id() ==
              active_input_methods[0]);

  manager_->RemoveObserver(&input_method_observer_);
}

// Tests if Shift+space selects the Korean layout.
IN_PROC_BROWSER_TEST_F(SystemKeyEventListenerTest, TestKoreanInputMethod) {
  manager_->AddObserver(&input_method_observer_);

  // Press Shift+space
  EXPECT_FALSE(SendFakeEvent(XK_Shift_R, 0, true));
  EXPECT_TRUE(SendFakeEvent(XK_space, ShiftMask, true));
  EXPECT_TRUE(SendFakeEvent(XK_space, ShiftMask, false));
  EXPECT_TRUE(SendFakeEvent(XK_Shift_R, ShiftMask, false));
  EXPECT_TRUE(input_method_observer_.current_input_method_id() ==
              active_input_methods[2]);

  // Press Control+space.
  EXPECT_FALSE(SendFakeEvent(XK_Control_L, 0, true));
  EXPECT_TRUE(SendFakeEvent(XK_space, ControlMask, true));
  EXPECT_TRUE(SendFakeEvent(XK_space, ControlMask, false));
  EXPECT_TRUE(SendFakeEvent(XK_Control_L, ControlMask, false));
  EXPECT_TRUE(input_method_observer_.current_input_method_id() ==
              active_input_methods[0]);

  // TODO(yusukes): Add tests for input method specific hot keys like XK_Henkan,
  // XK_Muhenkan, XK_ZenkakuHankaku, and XK_Hangul. This is not so easy since 1)
  // we have to enable "mozc-jp" and "mozc-hangul" that require ibus-daemon, and
  // 2) to let XKeysymToKeycode() handle e.g. XK_Henkan etc, we have to change
  // the XKB layout to "jp" first.

  manager_->RemoveObserver(&input_method_observer_);
}

}  // namespace chromeos
