// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_

#include <set>

#include "base/macros.h"
#include "ui/views/pointer_watcher.h"

namespace gfx {
class Point;
}

namespace ui {
class PointerEvent;
}

namespace ash {
class TrayBubbleWrapper;

// Handles events for a tray bubble, e.g. to close the system tray bubble when
// the user clicks outside it.
class TrayEventFilter : public views::PointerWatcher {
 public:
  TrayEventFilter();
  ~TrayEventFilter() override;

  void AddWrapper(TrayBubbleWrapper* wrapper);
  void RemoveWrapper(TrayBubbleWrapper* wrapper);

  // views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              views::Widget* target) override;

 private:
  void ProcessPressedEvent(const gfx::Point& location_in_screen,
                           views::Widget* target);

  std::set<TrayBubbleWrapper*> wrappers_;

  DISALLOW_COPY_AND_ASSIGN(TrayEventFilter);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
