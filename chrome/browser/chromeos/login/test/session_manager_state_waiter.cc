// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"

#include "base/run_loop.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace chromeos {

SessionStateWaiter::SessionStateWaiter(
    session_manager::SessionState target_state)
    : target_state_(target_state) {}

SessionStateWaiter::~SessionStateWaiter() = default;

void SessionStateWaiter::Wait() {
  if (session_manager::SessionManager::Get()->session_state() ==
      target_state_) {
    return;
  }
  session_observer_.Add(session_manager::SessionManager::Get());

  base::RunLoop run_loop;
  session_state_callback_ = run_loop.QuitClosure();
  run_loop.Run();

  session_observer_.RemoveAll();
}

void SessionStateWaiter::OnSessionStateChanged() {
  if (session_manager::SessionManager::Get()->session_state() ==
          target_state_ &&
      session_state_callback_) {
    std::move(session_state_callback_).Run();
  }
}

}  // namespace chromeos
