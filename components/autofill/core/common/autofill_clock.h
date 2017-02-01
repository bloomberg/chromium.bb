// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CLOCK_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CLOCK_H_

#include <memory>

#include "base/lazy_instance.h"
#include "base/macros.h"

namespace base {
class Clock;
class Time;
}  // namespace base

namespace autofill {

// Handles getting the current time for the Autofill feature. Can be injected
// with a customizable clock to facilitate testing.
class AutofillClock {
 public:
  // Returns the current time based on |clock_|.
  static base::Time Now();

 private:
  friend class TestAutofillClock;
  friend struct base::DefaultLazyInstanceTraits<AutofillClock>;

  // Resets a normal clock.
  static void SetClock();

  // Sets the clock to be used for tests.
  static void SetTestClock(std::unique_ptr<base::Clock> clock);

  AutofillClock();
  ~AutofillClock();

  // The clock used to return the current time.
  std::unique_ptr<base::Clock> clock_;

  DISALLOW_COPY_AND_ASSIGN(AutofillClock);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CLOCK_H_
