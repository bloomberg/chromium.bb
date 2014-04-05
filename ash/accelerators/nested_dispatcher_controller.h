// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_NESTED_DISPATCHER_CONTROLLER_H_
#define ASH_ACCELERATORS_NESTED_DISPATCHER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "ui/wm/public/dispatcher_client.h"

namespace ash {

class AcceleratorDispatcher;

// Creates a dispatcher which wraps another dispatcher.
// The outer dispatcher runs first and performs ash specific handling.
// If it does not consume the event it forwards the event to the nested
// dispatcher.
class ASH_EXPORT NestedDispatcherController
    : public aura::client::DispatcherClient {
 public:
  NestedDispatcherController();
  virtual ~NestedDispatcherController();

  // aura::client::DispatcherClient:
  virtual void RunWithDispatcher(
      base::MessagePumpDispatcher* dispatcher) OVERRIDE;
  virtual void QuitNestedMessageLoop() OVERRIDE;

 private:
  base::Closure quit_closure_;
  scoped_ptr<AcceleratorDispatcher> accelerator_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(NestedDispatcherController);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_NESTED_DISPATCHER_CONTROLLER_H_
