// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_dispatcher.h"

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

bool AcceleratorDispatcher::Dispatch(const MSG& msg) {
  // TODO(oshima): Consolidate win and linux.  http://crbug.com/116282
  if (!associated_window_)
    return false;
  if (!ui::IsNoopEvent(msg) && !associated_window_->CanReceiveEvents())
    return aura::Env::GetInstance()->GetDispatcher()->Dispatch(msg);

  if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN ||
      msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP) {
    ash::AcceleratorController* accelerator_controller =
        ash::Shell::GetInstance()->accelerator_controller();
    if (accelerator_controller) {
      ui::Accelerator accelerator(ui::KeyboardCodeFromNative(msg),
          ui::EventFlagsFromNative(msg) & kModifierMask);
      if (msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP)
        accelerator.set_type(ui::ET_KEY_RELEASED);
      if (accelerator_controller->Process(accelerator))
        return true;
      accelerator.set_type(aura::TranslatedKeyEvent(msg, false).type());
      if (accelerator_controller->Process(accelerator))
        return true;
    }
  }

  return nested_dispatcher_->Dispatch(msg);
}

}  // namespace ash
