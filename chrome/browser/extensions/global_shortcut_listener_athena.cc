// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/global_shortcut_listener_athena.h"

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  static GlobalShortcutListenerAthena* instance =
      new GlobalShortcutListenerAthena();
  return instance;
}

GlobalShortcutListenerAthena::GlobalShortcutListenerAthena() {
  // TODO: Compile out the CommandService on Athena. crbug.com/429398
}

GlobalShortcutListenerAthena::~GlobalShortcutListenerAthena() {
}

void GlobalShortcutListenerAthena::StartListening() {
  NOTIMPLEMENTED();
}

void GlobalShortcutListenerAthena::StopListening() {
  NOTIMPLEMENTED();
}

bool GlobalShortcutListenerAthena::RegisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  NOTIMPLEMENTED();
  return false;
}

void GlobalShortcutListenerAthena::UnregisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  NOTIMPLEMENTED();
}

}  // namespace extensions
