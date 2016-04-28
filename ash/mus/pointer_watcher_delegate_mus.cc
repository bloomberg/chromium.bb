// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/pointer_watcher_delegate_mus.h"

#include "ui/views/mus/window_manager_connection.h"

namespace ash {

PointerWatcherDelegateMus::PointerWatcherDelegateMus() {}

PointerWatcherDelegateMus::~PointerWatcherDelegateMus() {}

void PointerWatcherDelegateMus::AddPointerWatcher(
    views::PointerWatcher* watcher) {
  views::WindowManagerConnection::Get()->AddPointerWatcher(watcher);
}

void PointerWatcherDelegateMus::RemovePointerWatcher(
    views::PointerWatcher* watcher) {
  views::WindowManagerConnection::Get()->RemovePointerWatcher(watcher);
}

}  // namespace ash
