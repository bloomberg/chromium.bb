// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_mac.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

static base::LazyInstance<extensions::GlobalShortcutListenerMac> instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return instance.Pointer();
}

GlobalShortcutListenerMac::GlobalShortcutListenerMac()
    : is_listening_(false) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(implementor): Remove this.
  LOG(ERROR) << "GlobalShortcutListenerMac object created";
}

GlobalShortcutListenerMac::~GlobalShortcutListenerMac() {
  if (is_listening_)
    StopListening();
}

void GlobalShortcutListenerMac::StartListening() {
  DCHECK(!is_listening_);  // Don't start twice.
  NOTIMPLEMENTED();
  is_listening_ = true;
}

void GlobalShortcutListenerMac::StopListening() {
  DCHECK(is_listening_);  // No point if we are not already listening.
  NOTIMPLEMENTED();
  is_listening_ = false;
}

void GlobalShortcutListenerMac::RegisterAccelerator(
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

void GlobalShortcutListenerMac::UnregisterAccelerator(
    const ui::Accelerator& accelerator,
    GlobalShortcutListener::Observer* observer) {
  NOTIMPLEMENTED();
  // To implement:
  // 1) Unregister for the hotkey.
  // 2) Call base class UnregisterAccelerator.

  GlobalShortcutListener::UnregisterAccelerator(accelerator, observer);
}

}  // namespace extensions
