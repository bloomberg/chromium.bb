// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_H_
#define ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ash/ash_export.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {
class AcceleratorManager;
}

namespace ash {

class ScreenshotDelegate;
class CapsLockDelegate;

// AcceleratorController provides functions for registering or unregistering
// global keyboard accelerators, which are handled earlier than any windows. It
// also implements several handlers as an accelerator target.
class ASH_EXPORT AcceleratorController : public ui::AcceleratorTarget {
 public:
  AcceleratorController();
  virtual ~AcceleratorController();

  // Register a global keyboard accelerator for the specified target. If
  // multiple targets are registered for an accelerator, a target registered
  // later has higher priority.
  void Register(const ui::Accelerator& accelerator,
                ui::AcceleratorTarget* target);

  // Unregister the specified keyboard accelerator for the specified target.
  void Unregister(const ui::Accelerator& accelerator,
                  ui::AcceleratorTarget* target);

  // Unregister all keyboard accelerators for the specified target.
  void UnregisterAll(ui::AcceleratorTarget* target);

  // Activate the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  bool Process(const ui::Accelerator& accelerator);

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  void SetScreenshotDelegate(
      scoped_ptr<ScreenshotDelegate> screenshot_delegate);
  void SetCapsLockDelegate(scoped_ptr<CapsLockDelegate> caps_lock_delegate);

 private:
  // Initialize the accelerators this class handles as a target.
  void Init();

  scoped_ptr<ui::AcceleratorManager> accelerator_manager_;

  scoped_ptr<ScreenshotDelegate> screenshot_delegate_;
  scoped_ptr<CapsLockDelegate> caps_lock_delegate_;

  // A map from accelerators to the AcceleratorAction values, which are used in
  // the implementation.
  std::map<ui::Accelerator, int> accelerators_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorController);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_H_
