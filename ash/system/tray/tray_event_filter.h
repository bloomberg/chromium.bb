// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
#define ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_

#include "base/basictypes.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_handler.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

class TrayBubbleWrapper;

// Handles events for a tray bubble.

class TrayEventFilter : public ui::EventHandler {
 public:
  explicit TrayEventFilter(TrayBubbleWrapper* wrapper);
  virtual ~TrayEventFilter();

  // Overridden from ui::EventHandler.
  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

 private:
  // Returns true if the event is handled.
  bool ProcessLocatedEvent(ui::LocatedEvent* event);

  TrayBubbleWrapper* wrapper_;

  DISALLOW_COPY_AND_ASSIGN(TrayEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
