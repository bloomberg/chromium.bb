// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_DISPATCHER_H_
#define ASH_ACCELERATORS_ACCELERATOR_DISPATCHER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_dispatcher.h"

namespace ash {

// Dispatcher for handling accelerators from menu.
//
// Wraps a nested dispatcher to which control is passed if no accelerator key
// has been pressed. If the nested dispatcher is NULL, then the control is
// passed back to the default dispatcher.
// TODO(pkotwicz): Add support for a |nested_dispatcher| which sends
//  events to a system IME.
class ASH_EXPORT AcceleratorDispatcher : public base::MessagePumpDispatcher {
 public:
  explicit AcceleratorDispatcher(base::MessagePumpDispatcher* dispatcher);
  virtual ~AcceleratorDispatcher();

  // MessagePumpDispatcher overrides:
  virtual uint32_t Dispatch(const base::NativeEvent& event) OVERRIDE;

 private:
  base::MessagePumpDispatcher* nested_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorDispatcher);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_DISPATCHER_H_
