// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_H_
#define COMPONENTS_EXO_KEYBOARD_H_

#include <vector>

#include "base/macros.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wm_helper.h"
#include "ui/events/event_handler.h"

namespace ui {
enum class DomCode;
class KeyEvent;
}

namespace exo {
class KeyboardDelegate;
class KeyboardDeviceConfigurationDelegate;
class Surface;

// This class implements a client keyboard that represents one or more keyboard
// devices.
class Keyboard : public ui::EventHandler,
                 public WMHelper::FocusObserver,
                 public WMHelper::InputDeviceEventObserver,
                 public WMHelper::MaximizeModeObserver,
                 public SurfaceObserver {
 public:
  explicit Keyboard(KeyboardDelegate* delegate);
  ~Keyboard() override;

  bool HasDeviceConfigurationDelegate() const;
  void SetDeviceConfigurationDelegate(
      KeyboardDeviceConfigurationDelegate* delegate);

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Overridden WMHelper::FocusObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from ui::InputDeviceEventObserver:
  void OnKeyboardDeviceConfigurationChanged() override;

  // Overridden from WMHelper::MaximizeModeObserver:
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnding() override;
  void OnMaximizeModeEnded() override;

 private:
  // Returns the effective focus for |window|.
  Surface* GetEffectiveFocus(aura::Window* window) const;

  // The delegate instance that all events except for events about device
  // configuration are dispatched to.
  KeyboardDelegate* const delegate_;

  // The delegate instance that events about device configuration are dispatched
  // to.
  KeyboardDeviceConfigurationDelegate* device_configuration_delegate_ = nullptr;

  // The current focus surface for the keyboard.
  Surface* focus_ = nullptr;

  // Vector of currently pressed keys.
  std::vector<ui::DomCode> pressed_keys_;

  // Current set of modifier flags.
  int modifier_flags_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Keyboard);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_H_
