// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_H_
#define ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_H_

#include <map>
#include <set>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {
class AcceleratorManager;
}

namespace ash {

struct AcceleratorData;
class BrightnessControlDelegate;
class ImeControlDelegate;
class KeyboardBrightnessControlDelegate;
class ScreenshotDelegate;
class VolumeControlDelegate;

// Stores information about accelerator context, eg. previous accelerator
// or if the current accelerator is repeated or not.
class ASH_EXPORT AcceleratorControllerContext {
 public:
  AcceleratorControllerContext();
  ~AcceleratorControllerContext() {}

  // Updates context - determines if the accelerator is repeated, as well as
  // event type of the previous accelerator.
  void UpdateContext(const ui::Accelerator& accelerator);

  const ui::Accelerator& previous_accelerator() const {
    return previous_accelerator_;
  }
  bool repeated() const {
    return current_accelerator_ == previous_accelerator_ &&
        current_accelerator_.type() != ui::ET_UNKNOWN;
  }

 private:
  ui::Accelerator current_accelerator_;
  // Used for NEXT_IME and DISABLE_CAPS_LOCK accelerator actions.
  ui::Accelerator previous_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerContext);
};

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

  // Returns true if the |accelerator| is one of the |reserved_actions_|.
  bool IsReservedAccelerator(const ui::Accelerator& accelerator) const;

  // Performs the specified action. The |accelerator| may provide additional
  // data the action needs. Returns whether an action was performed
  // successfully.
  bool PerformAction(int action,
                     const ui::Accelerator& accelerator);

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  void SetBrightnessControlDelegate(
      scoped_ptr<BrightnessControlDelegate> brightness_control_delegate);
  void SetImeControlDelegate(
      scoped_ptr<ImeControlDelegate> ime_control_delegate);
  void SetScreenshotDelegate(
      scoped_ptr<ScreenshotDelegate> screenshot_delegate);
  BrightnessControlDelegate* brightness_control_delegate() const {
    return brightness_control_delegate_.get();
  }

  // Provides access to an object holding contextual information.
  AcceleratorControllerContext* context() {
    return &context_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(AcceleratorControllerTest, GlobalAccelerators);

  // Initializes the accelerators this class handles as a target.
  void Init();

  // Registers the specified accelerators.
  void RegisterAccelerators(const AcceleratorData accelerators[],
                            size_t accelerators_length);

  void SetKeyboardBrightnessControlDelegate(
      scoped_ptr<KeyboardBrightnessControlDelegate>
      keyboard_brightness_control_delegate);

  scoped_ptr<ui::AcceleratorManager> accelerator_manager_;

  // TODO(derat): BrightnessControlDelegate is also used by the system tray;
  // move it outside of this class.
  scoped_ptr<BrightnessControlDelegate> brightness_control_delegate_;
  scoped_ptr<ImeControlDelegate> ime_control_delegate_;
  scoped_ptr<KeyboardBrightnessControlDelegate>
      keyboard_brightness_control_delegate_;
  scoped_ptr<ScreenshotDelegate> screenshot_delegate_;

  // Contextual information, eg. if the current accelerator is repeated.
  AcceleratorControllerContext context_;

  // A map from accelerators to the AcceleratorAction values, which are used in
  // the implementation.
  std::map<ui::Accelerator, int> accelerators_;

  // Actions allowed when the user is not signed in.
  std::set<int> actions_allowed_at_login_screen_;
  // Actions allowed when the screen is locked.
  std::set<int> actions_allowed_at_lock_screen_;
  // Actions allowed when a modal window is up.
  std::set<int> actions_allowed_at_modal_window_;
  // Reserved actions. See accelerator_table.h for details.
  std::set<int> reserved_actions_;
  // Actions which will not be repeated while holding the accelerator key.
  std::set<int> nonrepeatable_actions_;
  // Actions allowed in app mode.
  std::set<int> actions_allowed_in_app_mode_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorController);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_H_
