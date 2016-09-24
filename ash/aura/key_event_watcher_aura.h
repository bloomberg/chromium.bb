// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_KEY_EVENT_WATCHER_AURA_H_
#define ASH_AURA_KEY_EVENT_WATCHER_AURA_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/common/key_event_watcher.h"
#include "ui/events/event_handler.h"

namespace ui {
class KeyEvent;
}

namespace ash {

// An EventHandler based KeyEventWatcher implementation.
class ASH_EXPORT KeyEventWatcherAura : public KeyEventWatcher,
                                       public ui::EventHandler {
 public:
  KeyEventWatcherAura();
  ~KeyEventWatcherAura() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyEventWatcherAura);
};

}  // namespace ash

#endif  // ASH_AURA_KEY_EVENT_WATCHER_AURA_H_
