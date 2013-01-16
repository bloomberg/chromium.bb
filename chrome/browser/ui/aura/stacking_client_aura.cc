// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/stacking_client_aura.h"

#include "ash/shell.h"
#include "ash/wm/stacking_controller.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_stacking_client.h"

StackingClientAura::StackingClientAura() {
  desktop_stacking_client_.reset(new views::DesktopStackingClient);
}

StackingClientAura::~StackingClientAura() {
}

aura::Window* StackingClientAura::GetDefaultParent(aura::Window* context,
                                                   aura::Window* window,
                                                   const gfx::Rect& bounds) {
#if defined(USE_ASH)
  if (chrome::GetHostDesktopTypeForNativeView(context) ==
      chrome::HOST_DESKTOP_TYPE_ASH) {
    return ash::Shell::GetInstance()->stacking_client()->GetDefaultParent(
        context, window, bounds);
  }
#endif
  return desktop_stacking_client_->GetDefaultParent(context, window, bounds);
}
