// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_initializer.h"

namespace chromeos {

namespace tether {

FakeInitializer::FakeInitializer(bool has_asynchronous_shutdown)
    : has_asynchronous_shutdown_(has_asynchronous_shutdown) {}

FakeInitializer::~FakeInitializer() {}

void FakeInitializer::FinishAsynchronousShutdown() {
  DCHECK(status() == Initializer::Status::SHUTTING_DOWN);
  TransitionToStatus(Initializer::Status::SHUT_DOWN);
}

void FakeInitializer::RequestShutdown() {
  DCHECK(status() == Initializer::Status::ACTIVE);

  if (has_asynchronous_shutdown_)
    TransitionToStatus(Initializer::Status::SHUTTING_DOWN);
  else
    TransitionToStatus(Initializer::Status::SHUT_DOWN);
}

}  // namespace tether

}  // namespace chromeos
