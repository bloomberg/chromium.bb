// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/disconnected_app_handler.h"

#include "ash/common/shell_window_ids.h"
#include "ash/mus/bridge/wm_window_mus.h"

namespace ash {
namespace mus {
namespace {

bool IsContainer(::ui::Window* window) {
  return WmWindowMus::Get(window)->IsContainer();
}

}  // namespace

DisconnectedAppHandler::DisconnectedAppHandler(::ui::Window* root_window) {
  WmWindowMus* root = WmWindowMus::Get(root_window);
  for (int shell_window_id = kShellWindowId_Min;
       shell_window_id < kShellWindowId_Max; ++shell_window_id) {
    // kShellWindowId_VirtualKeyboardContainer is lazily created.
    // TODO(sky): http://crbug.com/616909 .
    // kShellWindowId_PhantomWindow is not a container, but a window.
    if (shell_window_id == kShellWindowId_VirtualKeyboardContainer ||
        shell_window_id == kShellWindowId_PhantomWindow)
      continue;

// kShellWindowId_MouseCursorContainer is chromeos specific.
#if !defined(OS_CHROMEOS)
    if (shell_window_id == kShellWindowId_MouseCursorContainer)
      continue;
#endif

    WmWindowMus* container = WmWindowMus::AsWmWindowMus(
        root->GetChildByShellWindowId(shell_window_id));
    Add(container->mus_window());

    // Add any pre-existing windows in the container to
    // |disconnected_app_handler_|.
    for (::ui::Window* child : container->mus_window()->children()) {
      if (!IsContainer(child))
        Add(child);
    }
  }
}

DisconnectedAppHandler::~DisconnectedAppHandler() {}

void DisconnectedAppHandler::OnWindowEmbeddedAppDisconnected(
    ::ui::Window* window) {
  if (!IsContainer(window))
    window->Destroy();
}

void DisconnectedAppHandler::OnTreeChanging(const TreeChangeParams& params) {
  if (params.old_parent == params.receiver && IsContainer(params.old_parent))
    Remove(params.target);

  if (params.new_parent == params.receiver && IsContainer(params.new_parent))
    Add(params.target);

  ::ui::WindowTracker::OnTreeChanging(params);
}

}  // namespace mus
}  // namespace ash
