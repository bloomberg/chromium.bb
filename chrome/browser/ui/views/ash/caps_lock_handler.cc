// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/caps_lock_handler.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

// TODO(yusukes): Support Ash on Windows.
#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#endif

#if defined(OS_CHROMEOS)
CapsLockHandler::CapsLockHandler(chromeos::input_method::XKeyboard* xkeyboard)
    : xkeyboard_(xkeyboard),
      is_running_on_chromeos_(base::chromeos::IsRunningOnChromeOS()),
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
  if (is_running_on_chromeos_ &&
      // When spoken feedback is enabled, the Search key is used as an
      // accessibility modifier key.
      !g_browser_process->local_state()->GetBoolean(
          prefs::kSpokenFeedbackEnabled)) {
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
