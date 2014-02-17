// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/keyboard_driven_event_rewriter.h"

#include <X11/Xlib.h>

#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "ui/events/event_utils.h"

namespace chromeos {

namespace {

const int kModifierMask = ui::EF_SHIFT_DOWN;

// Returns true if and only if it is on login screen (i.e. user is not logged
// in) and the keyboard driven flag in the OEM manifest is on.
bool ShouldStripModifiersForArrowKeysAndEnter() {
  if (UserManager::IsInitialized() &&
      !UserManager::Get()->IsUserLoggedIn()) {
    return system::InputDeviceSettings::Get()
        ->ForceKeyboardDrivenUINavigation();
  }

  return false;
}

}  // namespace

KeyboardDrivenEventRewriter::KeyboardDrivenEventRewriter() {}

KeyboardDrivenEventRewriter::~KeyboardDrivenEventRewriter() {}

bool KeyboardDrivenEventRewriter::RewriteIfKeyboardDrivenOnLoginScreen(
    XEvent* event) {
  if (!ShouldStripModifiersForArrowKeysAndEnter())
    return false;

  return RewriteEvent(event);
}

bool KeyboardDrivenEventRewriter::RewriteForTesting(XEvent* event) {
  return RewriteEvent(event);
}

bool KeyboardDrivenEventRewriter::RewriteEvent(XEvent* event) {
  int flags = ui::EventFlagsFromNative(event);
  if ((flags & kModifierMask) != kModifierMask)
    return false;
  ui::KeyboardCode key_code = ui::KeyboardCodeFromNative(event);

  if (key_code != ui::VKEY_LEFT &&
      key_code != ui::VKEY_RIGHT &&
      key_code != ui::VKEY_UP &&
      key_code != ui::VKEY_DOWN &&
      key_code != ui::VKEY_RETURN &&
      key_code != ui::VKEY_F6) {
    return false;
  }

  XKeyEvent* xkey = &(event->xkey);
  xkey->state &= ~(ControlMask | Mod1Mask | ShiftMask);
  return true;
}

}  // namespace chromeos
