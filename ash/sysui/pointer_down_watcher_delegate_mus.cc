// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sysui/pointer_down_watcher_delegate_mus.h"

#include "ui/views/mus/window_manager_connection.h"

namespace ash {

PointerDownWatcherDelegateMus::PointerDownWatcherDelegateMus() {}

PointerDownWatcherDelegateMus::~PointerDownWatcherDelegateMus() {}

void PointerDownWatcherDelegateMus::AddPointerDownWatcher(
    views::PointerDownWatcher* watcher) {
  views::WindowManagerConnection::Get()->AddPointerDownWatcher(watcher);
}

void PointerDownWatcherDelegateMus::RemovePointerDownWatcher(
    views::PointerDownWatcher* watcher) {
  views::WindowManagerConnection::Get()->RemovePointerDownWatcher(watcher);
}

}  // namespace ash
