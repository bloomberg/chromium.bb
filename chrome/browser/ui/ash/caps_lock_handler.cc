// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/caps_lock_handler.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

// TODO(yusukes): Support Ash on Windows.
#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#endif

#if defined(OS_CHROMEOS)
CapsLockHandler::CapsLockHandler(chromeos::input_method::XKeyboard* xkeyboard)
    : xkeyboard_(xkeyboard),
      is_running_on_chromeos_(base::chromeos::IsRunningOnChromeOS()),
      caps_lock_is_on_(xkeyboard_->CapsLockIsEnabled()) {
  chromeos::SystemKeyEventListener* system_event_listener =
      chromeos::SystemKeyEventListener::GetInstance();
  // SystemKeyEventListener should be instantiated when we're running production
  // code on Chrome OS.
  DCHECK(!is_running_on_chromeos_ || system_event_listener ||
         CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType));
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

bool CapsLockHandler::IsCapsLockEnabled() const {
#if defined(OS_CHROMEOS)
  return caps_lock_is_on_;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

void CapsLockHandler::SetCapsLockEnabled(bool enabled) {
#if defined(OS_CHROMEOS)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (is_running_on_chromeos_) {
    xkeyboard_->SetCapsLockEnabled(enabled);
    return;
  }
#else
  NOTIMPLEMENTED();
#endif
}

void CapsLockHandler::ToggleCapsLock() {
#if defined(OS_CHROMEOS)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (is_running_on_chromeos_) {
    xkeyboard_->SetCapsLockEnabled(!caps_lock_is_on_);
    return;
  }
#else
  NOTIMPLEMENTED();
#endif
}

#if defined(OS_CHROMEOS)
void CapsLockHandler::OnCapsLockChange(bool enabled) {
  caps_lock_is_on_ = enabled;
}
#endif
