// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_MODAL_CONTAINER_EVENT_FILTER_H_
#define ASH_WM_SYSTEM_MODAL_CONTAINER_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/events/event_handler.h"

namespace ash {

class SystemModalContainerEventFilterDelegate;

class ASH_EXPORT SystemModalContainerEventFilter : public ui::EventHandler {
 public:
  explicit SystemModalContainerEventFilter(
      SystemModalContainerEventFilterDelegate* delegate);
  virtual ~SystemModalContainerEventFilter();

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

 private:
  SystemModalContainerEventFilterDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SystemModalContainerEventFilter);
};

}  // namespace ash

#endif  // ASH_WM_SYSTEM_MODAL_CONTAINER_EVENT_FILTER_H_
