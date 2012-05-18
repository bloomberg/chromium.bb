// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_H_
#define ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_H_
#pragma once

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ash/ash_export.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {
class AcceleratorManager;
}

namespace ash {

class BrightnessControlDelegate;
class CapsLockDelegate;
class ImeControlDelegate;
class ScreenshotDelegate;
class VolumeControlDelegate;

// AcceleratorController provides functions for registering or unregistering
// global keyboard accelerators, which are handled earlier than any windows. It
// also implements several handlers as an accelerator target.
class ASH_EXPORT AcceleratorController : public ui::AcceleratorTarget {
 public:
  AcceleratorController();
  virtual ~AcceleratorController();

  // Registers a global keyboard accelerator for the specified target. If
  // multiple targets are registered for an accelerator, a target registered
  // later has higher priority.
  void Register(const ui::Accelerator& accelerator,
                ui::AcceleratorTarget* target);

  // Unregisters the specified keyboard accelerator for the specified target.
  void Unregister(const ui::Accelerator& accelerator,
                  ui::AcceleratorTarget* target);

  // Unregisters all keyboard accelerators for the specified target.
  void UnregisterAll(ui::AcceleratorTarget* target);

  // Activates the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  bool Process(const ui::Accelerator& accelerator);

  // Returns true if the |accelerator| is registered.
  bool IsRegistered(const ui::Accelerator& accelerator) const;

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  void SetBrightnessControlDelegate(
      scoped_ptr<BrightnessControlDelegate> brightness_control_delegate);
  void SetCapsLockDelegate(scoped_ptr<CapsLockDelegate> caps_lock_delegate);
  void SetImeControlDelegate(
      scoped_ptr<ImeControlDelegate> ime_control_delegate);
  void SetScreenshotDelegate(
      scoped_ptr<ScreenshotDelegate> screenshot_delegate);
  void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> volume_control_delegate);

  BrightnessControlDelegate* brightness_control_delegate() const {
    return brightness_control_delegate_.get();
  }

 private:
  // Initializes the accelerators this class handles as a target.
  void Init();

  // Switches to a 0-indexed (in order of creation) window.
  // A negative index switches to the last window in the list.
  void SwitchToWindow(int window);

  scoped_ptr<ui::AcceleratorManager> accelerator_manager_;

  // TODO(derat): BrightnessControlDelegate is also used by the system tray;
  // move it outside of this class.
  scoped_ptr<BrightnessControlDelegate> brightness_control_delegate_;
  scoped_ptr<CapsLockDelegate> caps_lock_delegate_;
  scoped_ptr<ImeControlDelegate> ime_control_delegate_;
  scoped_ptr<ScreenshotDelegate> screenshot_delegate_;
  scoped_ptr<VolumeControlDelegate> volume_control_delegate_;

  // A map from accelerators to the AcceleratorAction values, which are used in
  // the implementation.
  std::map<ui::Accelerator, int> accelerators_;

  // Actions allowed when the user is not signed in or screen is locked
  std::set<int> actions_allowed_at_login_screen_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorController);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_H_
