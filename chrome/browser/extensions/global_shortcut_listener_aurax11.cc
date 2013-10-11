// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_aurax11.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

static base::LazyInstance<extensions::GlobalShortcutListenerAuraX11> instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return instance.Pointer();
}

GlobalShortcutListenerAuraX11::GlobalShortcutListenerAuraX11()
    : is_listening_(false) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(implementor): Remove this.
  LOG(ERROR) << "GlobalShortcutListenerAuraX11 object created";
}

GlobalShortcutListenerAuraX11::~GlobalShortcutListenerAuraX11() {
  if (is_listening_)
    StopListening();
}

void GlobalShortcutListenerAuraX11::StartListening() {
  DCHECK(!is_listening_);  // Don't start twice.
  NOTIMPLEMENTED();
  is_listening_ = true;
}

void GlobalShortcutListenerAuraX11::StopListening() {
  DCHECK(is_listening_);  // No point if we are not already listening.
  NOTIMPLEMENTED();
  is_listening_ = false;
}

void GlobalShortcutListenerAuraX11::RegisterAccelerator(
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

void GlobalShortcutListenerAuraX11::UnregisterAccelerator(
    const ui::Accelerator& accelerator,
    GlobalShortcutListener::Observer* observer) {
  NOTIMPLEMENTED();
  // To implement:
  // 1) Unregister for the hotkey.
  // 2) Call base class UnregisterAccelerator.

  GlobalShortcutListener::UnregisterAccelerator(accelerator, observer);
}

}  // namespace extensions
