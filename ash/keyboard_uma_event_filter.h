// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UMA_EVENT_FILTER_H_
#define ASH_KEYBOARD_UMA_EVENT_FILTER_H_

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "ui/events/event_handler.h"

namespace ash {

// EventFilter for tracking keyboard-related metrics, which intercepts events
// before they are processed by the usual path and logs metrics about the
// events.
class KeyboardUMAEventFilter : public ui::EventHandler {
 public:
  KeyboardUMAEventFilter();
  virtual ~KeyboardUMAEventFilter();

  // ui::EventHandler overrides:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

 private:
  // The timestamp of the last character keystroke.
  base::TimeDelta last_keystroke_time_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardUMAEventFilter);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_UMA_EVENT_FILTER_H_
