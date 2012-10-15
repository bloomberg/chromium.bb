// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
#define ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_

#include "base/basictypes.h"
#include "ui/aura/event_filter.h"
#include "ui/base/events/event.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

class TrayBubbleWrapper;

// Handles events for a tray bubble.

class TrayEventFilter : public aura::EventFilter {
 public:
  explicit TrayEventFilter(TrayBubbleWrapper* wrapper);
  virtual ~TrayEventFilter();

  // Overridden from aura::EventFilter.
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

 private:
  // Returns true if the event is handled.
  bool ProcessLocatedEvent(aura::Window* target,
                           const ui::LocatedEvent& event);

  TrayBubbleWrapper* wrapper_;

  DISALLOW_COPY_AND_ASSIGN(TrayEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
