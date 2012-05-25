// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace {

bool CheckIfFullscreenWindowExists(aura::Window* window) {
  if (window->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_FULLSCREEN)
    return true;
  aura::Window::Windows children = window->children();
  for (aura::Window::Windows::const_iterator i = children.begin();
       i != children.end();
       ++i) {
    if (CheckIfFullscreenWindowExists(*i))
      return true;
  }
  return false;
}

}  // namespace

bool IsFullScreenMode() {
  // This is used only by notification_ui_manager.cc. On aura, notification
  // will be managed in panel. This is temporary to get certain feature running
  // until we implement it for aura.
  return CheckIfFullscreenWindowExists(ash::Shell::GetPrimaryRootWindow());
}
