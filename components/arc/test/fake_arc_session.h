// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_ARC_SESSION_H_
#define COMPONENTS_ARC_TEST_FAKE_ARC_SESSION_H_

#include <memory>

#include "base/macros.h"
#include "components/arc/arc_session.h"
#include "components/arc/arc_stop_reason.h"

namespace arc {

// A fake ArcSession that creates a local connection.
class FakeArcSession : public ArcSession {
 public:
  FakeArcSession();
  ~FakeArcSession() override;

  // ArcSession overrides:
  void Start() override;
  void Stop() override;
  void OnShutdown() override;

  // To emulate unexpected stop, such as crash.
  void StopWithReason(ArcStopReason reason);

  // The following control Start() behavior for testing various situations.

  // Enables/disables boot failure emulation, in which OnStopped(reason) will
  // be called when Start() is called.
  void EnableBootFailureEmulation(ArcStopReason reason);

  // Emulate Start() is suspended at some phase, before OnReady() is invoked.
  void SuspendBoot();

  // Returns FakeArcSession instance. This can be used for a factory
  // in ArcBridgeServiceImpl.
  static std::unique_ptr<ArcSession> Create();

 private:
  bool boot_failure_emulation_enabled_ = false;
  ArcStopReason boot_failure_reason_;

  bool boot_suspended_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeArcSession);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ARC_SESSION_H_
