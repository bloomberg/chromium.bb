// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_DISPATCHER_H_
#define ASH_ACCELERATORS_ACCELERATOR_DISPATCHER_H_

#include "ash/ash_export.h"
#include "base/message_loop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// Dispatcher for handling accelerators from menu.
//
// Wraps a nested dispatcher to which control is passed if no accelerator key
// has been pressed.
// TODO(pkotwicz): Port AcceleratorDispatcher to mac.
// TODO(pkotwicz): Add support for a |nested_dispatcher| which sends
//  events to a system IME.
class ASH_EXPORT AcceleratorDispatcher : public base::MessageLoop::Dispatcher,
                                         public aura::WindowObserver {
 public:
  AcceleratorDispatcher(base::MessageLoop::Dispatcher* nested_dispatcher,
                        aura::Window* associated_window);
  virtual ~AcceleratorDispatcher();

  // MessageLoop::Dispatcher overrides:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

 private:
  base::MessageLoop::Dispatcher* nested_dispatcher_;

  // Window associated with |nested_dispatcher_| which is used to determine
  // whether the |nested_dispatcher_| is allowed to receive events.
  aura::Window* associated_window_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorDispatcher);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_DISPATCHER_H_
