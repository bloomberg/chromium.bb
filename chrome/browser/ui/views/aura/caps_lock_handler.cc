// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/caps_lock_handler.h"

#include "content/public/browser/browser_thread.h"

// TODO(yusukes): Support Ash on Windows.
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#endif

#if defined(OS_CHROMEOS)
CapsLockHandler::CapsLockHandler(chromeos::input_method::XKeyboard* xkeyboard)
    : xkeyboard_(xkeyboard),
      is_running_on_chromeos_(
          chromeos::system::runtime_environment::IsRunningOnChromeOS()),
      caps_lock_is_on_(xkeyboard_->CapsLockIsEnabled()) {
  chromeos::SystemKeyEventListener* system_event_listener =
      chromeos::SystemKeyEventListener::GetInstance();
  // SystemKeyEventListener should be instantiated when we're running on Chrome
  // OS.
  DCHECK(!is_running_on_chromeos_ || system_event_listener);
  if (system_event_listener)
    system_event_listener->AddCapsLockObserver(this);
}
#endif

CapsLockHandler::~CapsLockHandler() {
#if defined(OS_CHROMEOS)
  chromeos::SystemKeyEventListener* system_event_listener =
      chromeos::SystemKeyEventListener::GetInstance();
  if (system_event_listener)
    system_event_listener->RemoveCapsLockObserver(this);
#endif
}

bool CapsLockHandler::HandleToggleCapsLock() {
#if defined(OS_CHROMEOS)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (is_running_on_chromeos_) {
    // TODO(yusukes): Do not change Caps Lock status and just return false if
    // spoken feedback is enabled (crosbug.com/110127).
    xkeyboard_->SetCapsLockEnabled(!caps_lock_is_on_);
    return true;  // consume the shortcut key.
  }
#else
  NOTIMPLEMENTED();
#endif
  return false;
}

#if defined(OS_CHROMEOS)
void CapsLockHandler::OnCapsLockChange(bool enabled) {
  caps_lock_is_on_ = enabled;
}
#endif
