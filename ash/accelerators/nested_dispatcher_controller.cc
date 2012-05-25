// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/nested_dispatcher_controller.h"

#include "ash/accelerators/accelerator_dispatcher.h"
#include "ash/shell.h"

namespace ash {

NestedDispatcherController::NestedDispatcherController() {
  aura::client::SetDispatcherClient(Shell::GetPrimaryRootWindow(), this);
}

NestedDispatcherController::~NestedDispatcherController() {
}

void NestedDispatcherController::RunWithDispatcher(
    MessageLoop::Dispatcher* nested_dispatcher,
    aura::Window* associated_window,
    bool nestable_tasks_allowed) {
  MessageLoopForUI* loop = MessageLoopForUI::current();
  bool did_allow_task_nesting = loop->NestableTasksAllowed();
  loop->SetNestableTasksAllowed(nestable_tasks_allowed);

  AcceleratorDispatcher dispatcher(nested_dispatcher, associated_window);

  loop->RunWithDispatcher(&dispatcher);
  loop->SetNestableTasksAllowed(did_allow_task_nesting);
}

}  // namespace ash
