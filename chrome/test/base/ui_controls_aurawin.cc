// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/test/base/ui_controls.h"
#include "chrome/test/base/ui_controls_aura.h"
#include "chrome/test/base/ui_controls_internal_win.h"
#include "ui/aura/root_window.h"

namespace ui_controls {
namespace {
class UIControlsWin : public UIControlsAura {
 public:
  UIControlsWin() {}

  // UIControlsAura overrides:
  virtual bool SendKeyPress(gfx::NativeWindow native_window,
                            ui::KeyboardCode key,
                            bool control,
                            bool shift,
                            bool alt,
                            bool command) {
    DCHECK(!command);  // No command key on Aura
    HWND window = native_window->GetRootWindow()->GetAcceleratedWidget();
    return internal::SendKeyPressImpl(
        window, key, control, shift, alt, base::Closure());
  }
  virtual bool SendKeyPressNotifyWhenDone(gfx::NativeWindow native_window,
                                          ui::KeyboardCode key,
                                          bool control,
                                          bool shift,
                                          bool alt,
                                          bool command,
                                          const base::Closure& task) {
    DCHECK(!command);  // No command key on Aura
    HWND window = native_window->GetRootWindow()->GetAcceleratedWidget();
    return internal::SendKeyPressImpl(window, key, control, shift, alt, task);
  }
  virtual bool SendMouseMove(long x, long y) {
    gfx::Point point(x, y);
    return internal::SendMouseMoveImpl(point.x(), point.y(), base::Closure());
  }
  virtual bool SendMouseMoveNotifyWhenDone(long x,
                                           long y,
                                           const base::Closure& task) {
    gfx::Point point(x, y);
    return internal::SendMouseMoveImpl(point.x(), point.y(), task);
  }
  virtual bool SendMouseEvents(MouseButton type, int state) {
    return internal::SendMouseEventsImpl(type, state, base::Closure());
  }
  virtual bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                             int state,
                                             const base::Closure& task) {
    return internal::SendMouseEventsImpl(type, state, task);
  }
  virtual bool SendMouseClick(MouseButton type) {
    return SendMouseEvents(type, UP | DOWN);
  }
  virtual void RunClosureAfterAllPendingUIEvents(const base::Closure& closure) {
    // On windows, posting UI events is synchronous so just post the closure.
    MessageLoopForUI::current()->PostTask(FROM_HERE, closure);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UIControlsWin);
};

}  // namespace

UIControlsAura* CreateUIControlsAura(aura::RootWindow* root_window) {
  return new UIControlsWin();
}

}  // namespace ui_controls
