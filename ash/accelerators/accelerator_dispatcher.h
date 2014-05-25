// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_DISPATCHER_H_
#define ASH_ACCELERATORS_ACCELERATOR_DISPATCHER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class MessagePumpDispatcher;
class RunLoop;
}

namespace ui {
class KeyEvent;
}

namespace ash {

// Dispatcher for handling accelerators from menu.
//
// Wraps a nested dispatcher to which control is passed if no accelerator key
// has been pressed. If the nested dispatcher is NULL, then the control is
// passed back to the default dispatcher.
// TODO(pkotwicz): Add support for a |nested_dispatcher| which sends
//  events to a system IME.
class ASH_EXPORT AcceleratorDispatcher {
 public:
  virtual ~AcceleratorDispatcher() {}

  static scoped_ptr<AcceleratorDispatcher> Create(
      base::MessagePumpDispatcher* nested_dispatcher);

  // Creates a base::RunLoop object to run a nested message loop.
  virtual scoped_ptr<base::RunLoop> CreateRunLoop() = 0;

 protected:
  AcceleratorDispatcher() {}

  // Closes any open menu if the key-event could potentially be a system
  // accelerator.
  // Returns whether a menu was closed.
  bool MenuClosedForPossibleAccelerator(const ui::KeyEvent& key_event);

  // Attempts to trigger an accelerator for the key-event.
  // Returns whether an accelerator was triggered.
  bool AcceleratorProcessedForKeyEvent(const ui::KeyEvent& key_event);

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorDispatcher);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_DISPATCHER_H_
