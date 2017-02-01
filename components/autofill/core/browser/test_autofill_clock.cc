// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_clock.h"

#include <utility>

#include "base/test/simple_test_clock.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

TestAutofillClock::TestAutofillClock() {
  // Create a new test clock and set it as the AutofillClock clock and keep a
  // pointer to manipulate the time it returns.
  std::unique_ptr<base::SimpleTestClock> unique_test_clock(
      new base::SimpleTestClock());
  // Keep a pointer to the clock to be able to use its SetNow() function.
  test_clock_ = unique_test_clock.get();
  AutofillClock::SetTestClock(std::move(unique_test_clock));
}

TestAutofillClock::~TestAutofillClock() {
  // Destroys the test clock and resets a normal clock.
  AutofillClock::SetClock();
}

void TestAutofillClock::SetNow(base::Time now) {
  test_clock_->SetNow(now);
}

}  // namespace autofill
