// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_KEY_EVENT_WATCHER_H_
#define ASH_COMMON_KEY_EVENT_WATCHER_H_

#include <map>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {
class KeyEvent;
}

namespace ash {

// This class watches the key event given in the form of accelerator
// and calls the registered callback.
class ASH_EXPORT KeyEventWatcher {
 public:
  using KeyEventCallback = base::Callback<void(const ui::KeyEvent&)>;

  KeyEventWatcher();
  virtual ~KeyEventWatcher();

  // Add a callback that is called when the accelerator |accel| is
  // matched. The key event will be consumed if a matching accelerator
  // is found.
  void AddKeyEventCallback(const ui::Accelerator& accel,
                           const KeyEventCallback& callback);

 protected:
  // Finds and calls the registered callback whose accelerator matches
  // the |event|. Returns false if no accelerator is matched.
  bool HandleKeyEvent(const ui::KeyEvent& event);

 private:
  std::map<ui::Accelerator, KeyEventCallback> callback_map_;

  DISALLOW_COPY_AND_ASSIGN(KeyEventWatcher);
};

}  // namespace ash

#endif  // ASH_COMMON_KEY_EVENT_WATCHER_H_
