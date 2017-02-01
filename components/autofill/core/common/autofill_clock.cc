// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_clock.h"

#include <utility>

#include "base/time/clock.h"
#include "base/time/default_clock.h"

namespace autofill {

namespace {

static base::LazyInstance<AutofillClock> g_autofill_clock =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
base::Time AutofillClock::Now() {
  if (!g_autofill_clock.Get().clock_)
    SetClock();

  return g_autofill_clock.Get().clock_->Now();
}

AutofillClock::AutofillClock(){};
AutofillClock::~AutofillClock(){};

// static
void AutofillClock::SetClock() {
  g_autofill_clock.Get().clock_.reset(new base::DefaultClock());
}

// static
void AutofillClock::SetTestClock(std::unique_ptr<base::Clock> clock) {
  DCHECK(clock);
  g_autofill_clock.Get().clock_ = std::move(clock);
}

}  // namespace autofill
