// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/nested_dispatcher_controller.h"

#include "ash/accelerators/accelerator_dispatcher.h"
#include "ash/shell.h"
#include "base/run_loop.h"

namespace ash {

NestedDispatcherController::NestedDispatcherController() {
}

NestedDispatcherController::~NestedDispatcherController() {
}

void NestedDispatcherController::RunWithDispatcher(
    base::MessagePumpDispatcher* nested_dispatcher,
    aura::Window* associated_window) {
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);

  AcceleratorDispatcher dispatcher(nested_dispatcher, associated_window);

  // TODO(jbates) crbug.com/134753 Find quitters of this RunLoop and have them
  //              use run_loop.QuitClosure().
  base::RunLoop run_loop(&dispatcher);
  run_loop.Run();
}

}  // namespace ash
