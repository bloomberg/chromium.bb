// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_CAPS_LOCK_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_CAPS_LOCK_HANDLER_H_
#pragma once

#include "ash/caps_lock_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system_key_event_listener.h"

namespace chromeos {
namespace input_method {
class XKeyboard;
}  // namespace input_method
}  // namespace chromeos
#endif

// A class which toggles Caps Lock state when the hotkey for Caps Lock
// is pressed.
class CapsLockHandler : public ash::CapsLockDelegate
#if defined(OS_CHROMEOS)
    , public chromeos::SystemKeyEventListener::CapsLockObserver
#endif
{
 public:
  CapsLockHandler() {}
#if defined(OS_CHROMEOS)
  explicit CapsLockHandler(chromeos::input_method::XKeyboard* xkeyboard);
#endif
  virtual ~CapsLockHandler();

  // Overridden from ash::CapsLockHandler:
  virtual bool HandleToggleCapsLock() OVERRIDE;

#if defined(OS_CHROMEOS)
  // Overridden from chromeos::SystemKeyEventListener::CapsLockObserver:
  virtual void OnCapsLockChange(bool enabled) OVERRIDE;

  void set_is_running_on_chromeos_for_test(bool is_running_on_chromeos) {
    is_running_on_chromeos_ = is_running_on_chromeos;
  }

  bool caps_lock_is_on_for_test() const {
    return caps_lock_is_on_;
  }

 private:
  chromeos::input_method::XKeyboard* xkeyboard_;
  bool is_running_on_chromeos_;
  bool caps_lock_is_on_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(CapsLockHandler);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_CAPS_LOCK_HANDLER_H_
