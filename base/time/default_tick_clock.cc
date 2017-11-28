// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/default_tick_clock.h"

#include "base/lazy_instance.h"

namespace base {
namespace {
LazyInstance<DefaultTickClock>::Leaky g_instance = LAZY_INSTANCE_INITIALIZER;
}

DefaultTickClock::~DefaultTickClock() {}

TimeTicks DefaultTickClock::NowTicks() {
  return TimeTicks::Now();
}

// static
DefaultTickClock* DefaultTickClock::GetInstance() {
  return g_instance.Pointer();
}

}  // namespace base
