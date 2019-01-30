// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/testing/fake_clock.h"

namespace openscreen {

FakeClock::FakeClock(platform::TimeDelta now) : now_(now) {}
FakeClock::FakeClock(FakeClock& other) : now_(other.Now()) {}
FakeClock::~FakeClock() = default;

platform::TimeDelta FakeClock::Now() {
  return now_;
}

void FakeClock::Advance(platform::TimeDelta delta) {
  now_ += delta;
}

}  // namespace openscreen
