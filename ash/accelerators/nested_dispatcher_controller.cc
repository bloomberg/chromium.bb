// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/nested_dispatcher_controller.h"

#include "ash/accelerators/accelerator_dispatcher.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/run_loop.h"

namespace ash {

NestedDispatcherController::NestedDispatcherController() {
}

NestedDispatcherController::~NestedDispatcherController() {
}

void NestedDispatcherController::RunWithDispatcher(
    base::MessagePumpDispatcher* nested_dispatcher) {
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);

  scoped_ptr<AcceleratorDispatcher> old_accelerator_dispatcher =
      accelerator_dispatcher_.Pass();
  accelerator_dispatcher_ = AcceleratorDispatcher::Create(nested_dispatcher);

  // TODO(jbates) crbug.com/134753 Find quitters of this RunLoop and have them
  //              use run_loop.QuitClosure().
  scoped_ptr<base::RunLoop> run_loop = accelerator_dispatcher_->CreateRunLoop();
  base::AutoReset<base::Closure> reset_closure(&quit_closure_,
                                               run_loop->QuitClosure());
  run_loop->Run();
  accelerator_dispatcher_ = old_accelerator_dispatcher.Pass();
}

void NestedDispatcherController::QuitNestedMessageLoop() {
  CHECK(!quit_closure_.is_null());
  quit_closure_.Run();
  accelerator_dispatcher_.reset();
}

}  // namespace ash
