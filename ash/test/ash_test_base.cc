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
  helper_.InitRootWindow(Shell::GetRootWindow());
}

AshTestBase::~AshTestBase() {
  // Ensure that we don't use the previously-allocated static RootWindow object
  // later -- on Linux, it holds a reference to our message loop's X connection.
  aura::RootWindow::DeleteInstance();
}

void AshTestBase::SetUp() {
  helper_.SetUp();

  // Creates Shell and hook with Desktop.
  ash::Shell::CreateInstance(new TestShellDelegate);

  // Disable animations during tests.
  ui::LayerAnimator::set_disable_animations_for_test(true);
}

void AshTestBase::TearDown() {
  // Flush the message loop to finish pending release tasks.
  RunAllPendingInMessageLoop();

  // Tear down the shell.
  Shell::DeleteInstance();

  helper_.TearDown();
}

void AshTestBase::RunAllPendingInMessageLoop() {
  helper_.RunAllPendingInMessageLoop(Shell::GetRootWindow());
}

}  // namespace test
}  // namespace ash
