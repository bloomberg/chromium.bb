// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_session.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace arc {

FakeArcSession::FakeArcSession() = default;

FakeArcSession::~FakeArcSession() = default;

void FakeArcSession::Start() {
  if (boot_failure_emulation_enabled_) {
    for (auto& observer : observer_list_)
      observer.OnSessionStopped(boot_failure_reason_);
  } else if (!boot_suspended_) {
    for (auto& observer : observer_list_)
      observer.OnSessionReady();
  }
}

void FakeArcSession::Stop() {
  StopWithReason(ArcStopReason::SHUTDOWN);
}

void FakeArcSession::OnShutdown() {
  StopWithReason(ArcStopReason::SHUTDOWN);
}

void FakeArcSession::StopWithReason(ArcStopReason reason) {
  for (auto& observer : observer_list_)
    observer.OnSessionStopped(reason);
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
  return base::MakeUnique<FakeArcSession>();
}

}  // namespace arc
