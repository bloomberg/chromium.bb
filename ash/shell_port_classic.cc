// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_port_classic.h"

#include <memory>

#include "ash/pointer_watcher_adapter_classic.h"

namespace ash {

ShellPortClassic::ShellPortClassic() = default;

ShellPortClassic::~ShellPortClassic() = default;

void ShellPortClassic::Shutdown() {
  pointer_watcher_adapter_.reset();

  ShellPort::Shutdown();
}

void ShellPortClassic::AddPointerWatcher(
    views::PointerWatcher* watcher,
    views::PointerWatcherEventTypes events) {
  pointer_watcher_adapter_->AddPointerWatcher(watcher, events);
}

void ShellPortClassic::RemovePointerWatcher(views::PointerWatcher* watcher) {
  pointer_watcher_adapter_->RemovePointerWatcher(watcher);
}

void ShellPortClassic::CreatePointerWatcherAdapter() {
  pointer_watcher_adapter_ = std::make_unique<PointerWatcherAdapterClassic>();
}

}  // namespace ash
