// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/key_event_watcher.h"

namespace ash {

KeyEventWatcher::KeyEventWatcher() {}

KeyEventWatcher::~KeyEventWatcher() {}

void KeyEventWatcher::AddKeyEventCallback(const ui::Accelerator& key,
                                          const KeyEventCallback& callback) {
  callback_map_.insert(std::make_pair(key, callback));
}

bool KeyEventWatcher::HandleKeyEvent(const ui::KeyEvent& event) {
  auto iter = callback_map_.find(ui::Accelerator(event));
  if (iter != callback_map_.end()) {
    iter->second.Run(event);
    return true;
  }
  return false;
}

}  // namespace ash
