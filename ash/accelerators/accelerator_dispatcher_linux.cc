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
#include "ui/base/events.h"

namespace ash {

namespace {

const int kModifierMask = (ui::EF_SHIFT_DOWN |
                           ui::EF_CONTROL_DOWN |
                           ui::EF_ALT_DOWN);
}  // namespace

base::MessagePumpDispatcher::DispatchStatus AcceleratorDispatcher::Dispatch(
    XEvent* xev) {
  // TODO(oshima): Consolidate win and linux.  http://crbug.com/116282
  if (!associated_window_)
    return EVENT_QUIT;
  if (!ui::IsNoopEvent(xev) && !associated_window_->CanReceiveEvents())
    return aura::Env::GetInstance()->GetDispatcher()->Dispatch(xev);

  if (xev->type == KeyPress || xev->type == KeyRelease) {
    ash::AcceleratorController* accelerator_controller =
        ash::Shell::GetInstance()->accelerator_controller();
    if (accelerator_controller) {
      ui::Accelerator accelerator(ui::KeyboardCodeFromNative(xev),
          ui::EventFlagsFromNative(xev) & kModifierMask);
      if (xev->type == KeyRelease)
        accelerator.set_type(ui::ET_KEY_RELEASED);
      if (accelerator_controller->Process(accelerator))
        return EVENT_PROCESSED;

      accelerator.set_type(aura::TranslatedKeyEvent(xev, false).type());
      if (accelerator_controller->Process(accelerator))
        return EVENT_PROCESSED;
    }
  }
  return nested_dispatcher_->Dispatch(xev);
}

}  // namespace ash
