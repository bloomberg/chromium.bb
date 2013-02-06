// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/ui/ash/caps_lock_delegate_chromeos.h"

ash::CapsLockDelegate* ChromeShellDelegate::CreateCapsLockDelegate() {
  chromeos::input_method::XKeyboard* xkeyboard =
      chromeos::input_method::GetInputMethodManager()->GetXKeyboard();
  return new CapsLockDelegate(xkeyboard);
}
