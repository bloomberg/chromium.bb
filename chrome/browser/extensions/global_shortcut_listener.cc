// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/accelerators/accelerator.h"

using content::BrowserThread;

namespace extensions {

GlobalShortcutListener::GlobalShortcutListener() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GlobalShortcutListener::~GlobalShortcutListener() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(accelerator_map_.empty());  // Make sure we've cleaned up.
}

bool GlobalShortcutListener::RegisterAccelerator(
    const ui::Accelerator& accelerator, Observer* observer) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AcceleratorMap::const_iterator it = accelerator_map_.find(accelerator);
  if (it != accelerator_map_.end()) {
    // The accelerator has been registered.
    return false;
  }

  if (!RegisterAcceleratorImpl(accelerator)) {
    // If the platform-specific registration fails, mostly likely the shortcut
    // has been registered by other native applications.
    return false;
  }

  if (accelerator_map_.empty())
    StartListening();

  accelerator_map_[accelerator] = observer;
  return true;
}

void GlobalShortcutListener::UnregisterAccelerator(
    const ui::Accelerator& accelerator, Observer* observer) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AcceleratorMap::iterator it = accelerator_map_.find(accelerator);
  // We should never get asked to unregister something that we didn't register.
  DCHECK(it != accelerator_map_.end());
  // The caller should call this function with the right observer.
  DCHECK(it->second == observer);

  UnregisterAcceleratorImpl(accelerator);
  accelerator_map_.erase(it);
  if (accelerator_map_.empty())
    StopListening();
}

void GlobalShortcutListener::NotifyKeyPressed(
    const ui::Accelerator& accelerator) {
  AcceleratorMap::iterator iter = accelerator_map_.find(accelerator);
  if (iter == accelerator_map_.end()) {
    // This should never occur, because if it does, we have failed to unregister
    // or failed to clean up the map after unregistering the shortcut.
    NOTREACHED();
    return;  // No-one is listening to this key.
  }

  iter->second->OnKeyPressed(accelerator);
}

}  // namespace extensions
