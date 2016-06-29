// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_EVENT_HANDLER_AURA_H_
#define ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_EVENT_HANDLER_AURA_H_

#include "ash/common/wm/maximize_mode/maximize_mode_event_handler.h"
#include "ui/events/event_handler.h"

namespace ash {
namespace wm {

// Implementation of MaximizeModeEventHandler for aura. Uses ui::EventHandler.
class MaximizeModeEventHandlerAura : public MaximizeModeEventHandler,
                                     public ui::EventHandler {
 public:
  MaximizeModeEventHandlerAura();
  ~MaximizeModeEventHandlerAura() override;

 private:
  // ui::EventHandler override:
  void OnTouchEvent(ui::TouchEvent* event) override;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeEventHandlerAura);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_EVENT_HANDLER_AURA_H_
