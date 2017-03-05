// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WORKSPACE_EVENT_HANDLER_MUS_H_
#define ASH_MUS_BRIDGE_WORKSPACE_EVENT_HANDLER_MUS_H_

#include "ash/common/wm/workspace/workspace_event_handler.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace ash {
namespace mus {

// TODO(sky): investigate if can use aura version.
class WorkspaceEventHandlerMus : public WorkspaceEventHandler {
 public:
  explicit WorkspaceEventHandlerMus(aura::Window* workspace_window);
  ~WorkspaceEventHandlerMus() override;

  // Returns the WorkspaceEventHandlerMus associated with |window|, or null
  // if |window| is not the workspace window.
  static WorkspaceEventHandlerMus* Get(aura::Window* window);

  // Returns the window associated with the workspace.
  aura::Window* workspace_window() { return workspace_window_; }

 private:
  aura::Window* workspace_window_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventHandlerMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WORKSPACE_EVENT_HANDLER_MUS_H_
