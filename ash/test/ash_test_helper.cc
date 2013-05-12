// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/base/ime/text_input_test_support.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"


#if defined(ENABLE_MESSAGE_CENTER)
#include "ui/message_center/message_center.h"
#endif

namespace ash {
namespace test {

AshTestHelper::AshTestHelper(base::MessageLoopForUI* message_loop)
    : message_loop_(message_loop),
      test_shell_delegate_(NULL) {
  CHECK(message_loop_);
}

AshTestHelper::~AshTestHelper() {
}

void AshTestHelper::SetUp() {
  // TODO(jennyz): Create mock or test AudioHandler so we can instantiate
  // an ash::Shell with the new CrasAudioHandler. crbug.com/233266
  CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kAshDisableNewAudioHandler);
  // Disable animations during tests.
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  ui::TextInputTestSupport::Initialize();

  // Creates Shell and hook with Desktop.
  test_shell_delegate_ = new TestShellDelegate;

#if defined(ENABLE_MESSAGE_CENTER)
  // Creates MessageCenter since g_browser_process is not created in AshTestBase
  // tests.
  message_center::MessageCenter::Initialize();
#endif
  ash::Shell::CreateInstance(test_shell_delegate_);
  Shell* shell = Shell::GetInstance();
  test::DisplayManagerTestApi(shell->display_manager()).
      DisableChangeDisplayUponHostResize();
  ShellTestApi(shell).DisableOutputConfiguratorAnimation();

}

void AshTestHelper::TearDown() {
  // Tear down the shell.
  Shell::DeleteInstance();

#if defined(ENABLE_MESSAGE_CENTER)
  // Remove global message center state.
  message_center::MessageCenter::Shutdown();
#endif

  aura::Env::DeleteInstance();
  ui::TextInputTestSupport::Shutdown();

  zero_duration_mode_.reset();
}

void AshTestHelper::RunAllPendingInMessageLoop() {
#if !defined(OS_MACOSX)
  DCHECK(MessageLoopForUI::current() == message_loop_);
  base::RunLoop run_loop(aura::Env::GetInstance()->GetDispatcher());
  run_loop.RunUntilIdle();
#endif
}

aura::RootWindow* AshTestHelper::CurrentContext() {
  aura::RootWindow* root_window = Shell::GetActiveRootWindow();
  if (!root_window)
    root_window = Shell::GetPrimaryRootWindow();
  DCHECK(root_window);
  return root_window;
}

}  // namespace test
}  // namespace ash
