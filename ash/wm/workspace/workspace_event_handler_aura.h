// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_AURA_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_AURA_H_

#include "ash/ash_export.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}

namespace ash {

class ASH_EXPORT WorkspaceEventHandlerAura : public ui::EventHandler,
                                             public WorkspaceEventHandler {
 public:
  explicit WorkspaceEventHandlerAura(aura::Window* workspace_window);
  ~WorkspaceEventHandlerAura() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  aura::Window* workspace_window_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventHandlerAura);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_AURA_H_
