// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/accelerators/accelerator.h"

namespace extensions {

GlobalShortcutListener::GlobalShortcutListener() {
}

GlobalShortcutListener::~GlobalShortcutListener() {
  DCHECK(accelerator_map_.empty());  // Make sure we've cleaned up.
}

void GlobalShortcutListener::RegisterAccelerator(
    const ui::Accelerator& accelerator, Observer* observer) {
  AcceleratorMap::const_iterator it = accelerator_map_.find(accelerator);
  if (it == accelerator_map_.end()) {
    if (accelerator_map_.empty())
      GlobalShortcutListener::GetInstance()->StartListening();
    Observers* observers = new Observers;
    observers->AddObserver(observer);
    accelerator_map_[accelerator] = observers;
  } else {
    // Make sure we don't register the same accelerator twice.
    DCHECK(!accelerator_map_[accelerator]->HasObserver(observer));
    accelerator_map_[accelerator]->AddObserver(observer);
  }
}

void GlobalShortcutListener::UnregisterAccelerator(
    const ui::Accelerator& accelerator, Observer* observer) {
  AcceleratorMap::iterator it = accelerator_map_.find(accelerator);
  DCHECK(it != accelerator_map_.end());
  DCHECK(it->second->HasObserver(observer));
  it->second->RemoveObserver(observer);
  if (!it->second->might_have_observers()) {
    accelerator_map_.erase(it);
    if (accelerator_map_.empty())
      GlobalShortcutListener::GetInstance()->StopListening();
  }
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
  // The observer list should not be empty.
  DCHECK(iter->second->might_have_observers());

  FOR_EACH_OBSERVER(Observer, *(iter->second), OnKeyPressed(accelerator));
}

}  // namespace extensions
