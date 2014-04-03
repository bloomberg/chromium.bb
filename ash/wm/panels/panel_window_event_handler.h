// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PANELS_PANEL_WINDOW_EVENT_HANDLER_H_
#define ASH_WM_PANELS_PANEL_WINDOW_EVENT_HANDLER_H_

#include "ui/events/event_handler.h"

namespace ash {

// PanelWindowEventHandler minimizes panels when the user double clicks or
// double taps on the panel header.
class PanelWindowEventHandler : public ui::EventHandler {
 public:
  PanelWindowEventHandler();
  virtual ~PanelWindowEventHandler();

  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PanelWindowEventHandler);
};

}  // namespace ash

#endif  // ASH_WM_PANELS_PANEL_WINDOW_EVENT_HANDLER_H_
