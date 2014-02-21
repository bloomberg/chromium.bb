// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_watcher.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell/shell_delegate_impl.h"
#include "ash/system/user/login_status.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window_event_dispatcher.h"

namespace ash {

typedef test::AshTestBase WindowWatcherTest;

// This test verifies that shell can be torn down without causing failures
// bug http://code.google.com/p/chromium/issues/detail?id=130332
TEST_F(WindowWatcherTest, ShellDeleteInstance) {
  RunAllPendingInMessageLoop();
  scoped_ptr<ash::shell::WindowWatcher> window_watcher;
  Shell::DeleteInstance();

  shell::ShellDelegateImpl* delegate = new ash::shell::ShellDelegateImpl;
  Shell::CreateInstance(delegate);
  Shell::GetPrimaryRootWindow()->GetDispatcher()->host()->Show();
  Shell::GetInstance()->CreateShelf();
  Shell::GetInstance()->UpdateAfterLoginStatusChange(
      user::LOGGED_IN_USER);

  window_watcher.reset(new ash::shell::WindowWatcher);

  delegate->SetWatcher(window_watcher.get());
  Shell::GetPrimaryRootWindow()->Hide();
  window_watcher.reset();
  delegate->SetWatcher(NULL);
}

}  // namespace ash
