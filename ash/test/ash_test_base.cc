// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include "ash/shell.h"
#include "ash/test/test_shell_delegate.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/compositor/layer_animator.h"

namespace ash {
namespace test {

AshTestBase::AshTestBase() {
}

AshTestBase::~AshTestBase() {
}

void AshTestBase::SetUp() {
  // Creates Shell and hook with Desktop.
  TestShellDelegate* delegate = new TestShellDelegate;
  ash::Shell::CreateInstance(delegate);

  helper_.SetUp();
  helper_.InitRootWindow(Shell::GetRootWindow());

  // Disable animations during tests.
  ui::LayerAnimator::set_disable_animations_for_test(true);
}

void AshTestBase::TearDown() {
  // Flush the message loop to finish pending release tasks.
  RunAllPendingInMessageLoop();

  helper_.TearDown();

  // Tear down the shell.
  Shell::DeleteInstance();
}

void AshTestBase::RunAllPendingInMessageLoop() {
  helper_.RunAllPendingInMessageLoop(Shell::GetRootWindow());
}

}  // namespace test
}  // namespace ash
