// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_dispatcher.h"

#include <X11/Xlib.h>

// Xlib defines RootWindow
#ifdef RootWindow
#undef RootWindow
#endif

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ui/aura/env.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

namespace {

const int kModifierMask = (ui::EF_SHIFT_DOWN |
                           ui::EF_CONTROL_DOWN |
                           ui::EF_ALT_DOWN);
}  // namespace

base::MessagePumpDispatcher::DispatchStatus AcceleratorDispatcher::Dispatch(
    XEvent* xev) {
  if (!associated_window_)
    return EVENT_QUIT;
  if (!associated_window_->CanReceiveEvents())
    return aura::Env::GetInstance()->GetDispatcher()->Dispatch(xev);

  if (xev->type == KeyPress) {
    ash::AcceleratorController* accelerator_controller =
        ash::Shell::GetInstance()->accelerator_controller();
    ui::Accelerator accelerator(ui::KeyboardCodeFromNative(xev),
                                ui::EventFlagsFromNative(xev) & kModifierMask);
    if (accelerator_controller && accelerator_controller->Process(accelerator))
      return EVENT_PROCESSED;
  }
  return nested_dispatcher_->Dispatch(xev);
}

}  // namespace ash
