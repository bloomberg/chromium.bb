// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_EVENT_HANDLER_H_
#define ASH_COMMON_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_EVENT_HANDLER_H_

#include "base/macros.h"

namespace ui {
class TouchEvent;
}

namespace ash {
namespace wm {

// MaximizeModeEventHandler handles toggling fullscreen when appropriate.
// MaximizeModeEventHandler installs event handlers in an environment specific
// way, e.g. EventHandler for aura.
class MaximizeModeEventHandler {
 public:
  MaximizeModeEventHandler();
  virtual ~MaximizeModeEventHandler();

 protected:
  // Subclasses call this to toggle fullscreen. If a toggle happened returns
  // true.
  bool ToggleFullscreen(const ui::TouchEvent& event);

 private:
  DISALLOW_COPY_AND_ASSIGN(MaximizeModeEventHandler);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_EVENT_HANDLER_H_
