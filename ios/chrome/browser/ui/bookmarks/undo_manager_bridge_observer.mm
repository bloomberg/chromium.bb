// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/bookmarks/undo_manager_bridge_observer.h"

#include "base/logging.h"

namespace bookmarks {
UndoManagerBridge::UndoManagerBridge(id<UndoManagerBridgeObserver> observer)
    : observer_(observer) {
  DCHECK(observer);
}

void UndoManagerBridge::OnUndoManagerStateChange() {
  [observer_ undoManagerChanged];
}
}  // namespace bookmarks
