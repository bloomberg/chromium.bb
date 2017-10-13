// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_session.h"

#include <memory>

#include "base/logging.h"

namespace arc {

FakeArcSession::FakeArcSession() = default;

FakeArcSession::~FakeArcSession() = default;

void FakeArcSession::StartForLoginScreen() {
  is_for_login_screen_ = true;
  if (boot_failure_emulation_enabled_) {
    for (auto& observer : observer_list_)
      observer.OnSessionStopped(boot_failure_reason_, false);
  }
}

bool FakeArcSession::IsForLoginScreen() {
  return is_for_login_screen_;
}

void FakeArcSession::Start() {
  is_for_login_screen_ = false;
  if (boot_failure_emulation_enabled_) {
    for (auto& observer : observer_list_)
      observer.OnSessionStopped(boot_failure_reason_, false);
  } else if (!boot_suspended_) {
    running_ = true;
  }
}

bool FakeArcSession::IsRunning() {
  return running_;
}

void FakeArcSession::Stop() {
  stop_requested_ = true;
  StopWithReason(ArcStopReason::SHUTDOWN);
}

void FakeArcSession::OnShutdown() {
  StopWithReason(ArcStopReason::SHUTDOWN);
}

bool FakeArcSession::IsStopRequested() {
  return stop_requested_;
}

void FakeArcSession::StopWithReason(ArcStopReason reason) {
  bool was_mojo_connected = running_;
  running_ = false;
  for (auto& observer : observer_list_)
    observer.OnSessionStopped(reason, was_mojo_connected);
}

void FakeArcSession::EnableBootFailureEmulation(ArcStopReason reason) {
  DCHECK(!boot_failure_emulation_enabled_);
  DCHECK(!boot_suspended_);

  boot_failure_emulation_enabled_ = true;
  boot_failure_reason_ = reason;
}

void FakeArcSession::SuspendBoot() {
  DCHECK(!boot_failure_emulation_enabled_);
  DCHECK(!boot_suspended_);

  boot_suspended_ = true;
}

// static
std::unique_ptr<ArcSession> FakeArcSession::Create() {
  return std::make_unique<FakeArcSession>();
}

}  // namespace arc
