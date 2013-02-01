// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_util.h"

#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shell.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace launcher {

int GetBrowserItemIndex(const LauncherModel& launcher_model) {
  for (size_t i = 0; i < launcher_model.items().size(); i++) {
    if (launcher_model.items()[i].type == ash::TYPE_BROWSER_SHORTCUT)
      return i;
  }
  return -1;
}

void MoveToEventRootIfPanel(aura::Window* maybe_panel,
                            const ui::Event& event) {
  if (maybe_panel->type() != aura::client::WINDOW_TYPE_PANEL)
    return;
  views::View* target = static_cast<views::View*>(event.target());
  aura::RootWindow* target_root =
      target ? target->GetWidget()->GetNativeView()->GetRootWindow() : NULL;
  if (target_root && target_root != maybe_panel->GetRootWindow()) {
    aura::Window* panel_container =
        ash::Shell::GetContainer(target_root, maybe_panel->parent()->id());
    // Move the panel to the target launcher.
    panel_container->AddChild(maybe_panel);
  }
}

}  // namespace launcher
}  // namespace ash
