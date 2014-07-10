// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_INPUT_PUBLIC_ACCELERATOR_MANAGER_H_
#define ATHENA_INPUT_PUBLIC_ACCELERATOR_MANAGER_H_

#include "athena/athena_export.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
class Accelerator;
}

namespace views {
class FocusManager;
}

namespace athena {

enum TriggerEvent {
  TRIGGER_ON_PRESS,
  TRIGGER_ON_RELEASE,
};

// Accelerator flags.
enum AcceleratorFlags {
  AF_NONE = 0,
  // Used for accelerators that should not be fired on auto repeated
  // key event, such as toggling fullscrren.
  AF_NON_AUTO_REPEATABLE = 1 << 0,
  // Most key events are sent to applications first as they may
  // want to consume them. Reserverd accelerators are reserved for OS
  // and cannot be consumed by apps. (such as window cycling)
  AF_RESERVED = 1 << 1,
  // Used for accelerators that are useful only in debug mode.
  AF_DEBUG = 1 << 2,
};

struct AcceleratorData {
  // true if the accelerator should be triggered upon ui::ET_KEY_PRESSED
  TriggerEvent trigger_event;
  ui::KeyboardCode keycode;  // KeyEvent event flags.
  int keyevent_flags;        // Combination of ui::KeyEventFlags
  int command_id;            // ID to distinguish
  int accelerator_flags;     // Combination of AcceleratorFlags;
};

// An interface that implements behavior for the set of
// accelerators.
class ATHENA_EXPORT AcceleratorHandler {
 public:
  virtual ~AcceleratorHandler() {}

  virtual bool IsCommandEnabled(int command_id) const = 0;
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) = 0;
};

class ATHENA_EXPORT AcceleratorManager {
 public:
  // Returns an AccelerarManager for global acelerators.
  static AcceleratorManager* Get();

  // Creates an AcceleratorManager for application windows that
  // define their own accelerators.
  static scoped_ptr<AcceleratorManager> CreateForFocusManager(
      views::FocusManager* focus_manager);

  virtual ~AcceleratorManager() {}

  // Tells if the accelerator is registered with the given flag.  If
  // flags is AF_NONE, it simply tells if the accelerator is
  // registered with any flags.
  virtual bool IsRegistered(const ui::Accelerator& accelerator,
                            int flags) const = 0;

  // Register accelerators and its handler that will be invoked when
  // one of accelerator is fired.
  virtual void RegisterAccelerators(const AcceleratorData accelerators[],
                                    size_t num_accelerators,
                                    AcceleratorHandler* handler) = 0;

  // Enables/Disables accelerators that has a AF_DEBUG flag.
  virtual void SetDebugAcceleratorsEnabled(bool enabled) = 0;
};

}  // namespace athena

#endif  // ATHENA_INPUT_PUBLIC_ACCELERATOR_MANAGER_H_
