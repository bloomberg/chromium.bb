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
    base::MessageLoop::Dispatcher* nested_dispatcher,
    aura::Window* associated_window,
    bool nestable_tasks_allowed) {
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  bool did_allow_task_nesting = loop->NestableTasksAllowed();
  loop->SetNestableTasksAllowed(nestable_tasks_allowed);

  AcceleratorDispatcher dispatcher(nested_dispatcher, associated_window);

  // TODO(jbates) crbug.com/134753 Find quitters of this RunLoop and have them
  //              use run_loop.QuitClosure().
  base::RunLoop run_loop(&dispatcher);
  run_loop.Run();
  loop->SetNestableTasksAllowed(did_allow_task_nesting);
}

}  // namespace ash
