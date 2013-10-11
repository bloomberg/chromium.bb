// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_chromeos.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

static base::LazyInstance<extensions::GlobalShortcutListenerChromeOS> instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return instance.Pointer();
}

GlobalShortcutListenerChromeOS::GlobalShortcutListenerChromeOS()
    : is_listening_(false) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(implementor): Remove this.
  LOG(ERROR) << "GlobalShortcutListenerChromeOS object created";
}

GlobalShortcutListenerChromeOS::~GlobalShortcutListenerChromeOS() {
  if (is_listening_)
    StopListening();
}

void GlobalShortcutListenerChromeOS::StartListening() {
  DCHECK(!is_listening_);  // Don't start twice.
  NOTIMPLEMENTED();
  is_listening_ = true;
}

void GlobalShortcutListenerChromeOS::StopListening() {
  DCHECK(is_listening_);  // No point if we are not already listening.
  NOTIMPLEMENTED();
  is_listening_ = false;
}

void GlobalShortcutListenerChromeOS::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    GlobalShortcutListener::Observer* observer) {
  NOTIMPLEMENTED();
  // To implement:
  // 1) Convert modifiers to platform specific modifiers.
  // 2) Register for the hotkey.
  // 3) If not successful, log why.
  // 4) Else, call base class RegisterAccelerator.

  GlobalShortcutListener::RegisterAccelerator(accelerator, observer);
}

void GlobalShortcutListenerChromeOS::UnregisterAccelerator(
    const ui::Accelerator& accelerator,
    GlobalShortcutListener::Observer* observer) {
  NOTIMPLEMENTED();
  // To implement:
  // 1) Unregister for the hotkey.
  // 2) Call base class UnregisterAccelerator.

  GlobalShortcutListener::UnregisterAccelerator(accelerator, observer);
}

}  // namespace extensions
