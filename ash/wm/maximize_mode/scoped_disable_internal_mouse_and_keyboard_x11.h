// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_SCOPED_DISABLE_INTERNAL_MOUSE_AND_KEYBOARD_X11_H_
#define ASH_WM_MAXIMIZE_MODE_SCOPED_DISABLE_INTERNAL_MOUSE_AND_KEYBOARD_X11_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/common/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "base/macros.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

// Identifies which input devices are internal and provides a helper function to
// test if an input event came from an internal device.
class ScopedDisableInternalMouseAndKeyboardX11
    : public ScopedDisableInternalMouseAndKeyboard,
      public ui::PlatformEventObserver {
 public:
  ScopedDisableInternalMouseAndKeyboardX11();
  ~ScopedDisableInternalMouseAndKeyboardX11() override;

  // ui::PlatformEventObserver:
  void WillProcessEvent(const ui::PlatformEvent& event) override;
  void DidProcessEvent(const ui::PlatformEvent& event) override;

 private:
  int touchpad_device_id_;
  int keyboard_device_id_;
  int core_keyboard_device_id_;

  // Tracks the last known mouse cursor location caused before blocking the
  // internal touchpad or caused by an external mouse.
  gfx::Point last_mouse_location_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDisableInternalMouseAndKeyboardX11);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_SCOPED_DISABLE_INTERNAL_MOUSE_AND_KEYBOARD_X11_H_
