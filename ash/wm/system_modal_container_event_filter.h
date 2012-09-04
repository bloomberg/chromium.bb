// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_MODAL_CONTAINER_EVENT_FILTER_H_
#define ASH_WM_SYSTEM_MODAL_CONTAINER_EVENT_FILTER_H_

#include "base/compiler_specific.h"
#include "ui/aura/event_filter.h"
#include "ash/ash_export.h"

namespace ash {
namespace internal {

class SystemModalContainerEventFilterDelegate;

class ASH_EXPORT SystemModalContainerEventFilter : public aura::EventFilter {
 public:
  explicit SystemModalContainerEventFilter(
      SystemModalContainerEventFilterDelegate* delegate);
  virtual ~SystemModalContainerEventFilter();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

 private:
  SystemModalContainerEventFilterDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SystemModalContainerEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SYSTEM_MODAL_CONTAINER_EVENT_FILTER_H_
