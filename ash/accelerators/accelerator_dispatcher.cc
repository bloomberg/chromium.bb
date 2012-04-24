// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_dispatcher.h"

#if defined(USE_X11)
#include <X11/Xlib.h>

// Xlib defines RootWindow
#ifdef RootWindow
#undef RootWindow
#endif
#endif  // defined(USE_X11)

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
#if defined(OS_WIN)
bool IsKeyEvent(const MSG& msg) {
  return
      msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN ||
      msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP;
}
bool IsKeyRelease(const MSG& msg) {
  return msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP;
}
#elif defined(USE_X11)
bool IsKeyEvent(const XEvent* xev) {
  return xev->type == KeyPress || xev->type == KeyRelease;
}
bool IsKeyRelease(const XEvent* xev) {
  return xev->type == KeyRelease;
}
#endif

}  // namespace

AcceleratorDispatcher::AcceleratorDispatcher(
    MessageLoop::Dispatcher* nested_dispatcher, aura::Window* associated_window)
    : nested_dispatcher_(nested_dispatcher),
      associated_window_(associated_window) {
  DCHECK(nested_dispatcher_);
  associated_window_->AddObserver(this);
}

AcceleratorDispatcher::~AcceleratorDispatcher() {
  if (associated_window_)
    associated_window_->RemoveObserver(this);
}

void AcceleratorDispatcher::OnWindowDestroying(aura::Window* window) {
  if (associated_window_ == window)
    associated_window_ = NULL;
}

bool AcceleratorDispatcher::Dispatch(const base::NativeEvent& event) {
  if (!associated_window_)
    return false;
  if (!ui::IsNoopEvent(event) && !associated_window_->CanReceiveEvents())
    return aura::Env::GetInstance()->GetDispatcher()->Dispatch(event);

  if (IsKeyEvent(event)) {
    ash::AcceleratorController* accelerator_controller =
        ash::Shell::GetInstance()->accelerator_controller();
    if (accelerator_controller) {
      ui::Accelerator accelerator(ui::KeyboardCodeFromNative(event),
          ui::EventFlagsFromNative(event) & kModifierMask);
      if (IsKeyRelease(event))
        accelerator.set_type(ui::ET_KEY_RELEASED);
      if (accelerator_controller->Process(accelerator))
        return true;
    }
  }

  return nested_dispatcher_->Dispatch(event);
}

}  // namespace ash
