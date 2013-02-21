// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SIMPLE_TEST_CLOCK_H_
#define BASE_SIMPLE_TEST_CLOCK_H_

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "base/time/clock.h"

namespace base {

// SimpleTestClock is a Clock implementation that gives control over
// the returned Time objects.  All methods can be called from any
// thread.
class SimpleTestClock : public Clock {
 public:
  // Starts off with a clock set to Time().
  SimpleTestClock();
  virtual ~SimpleTestClock();

  virtual Time Now() OVERRIDE;

  // Sets the current time forward by |delta|.  Safe to call from any
  // thread.
  void Advance(TimeDelta delta);

 private:
  // Protects |now_|.
  Lock lock_;

  Time now_;
};

}  // namespace base

#endif  // BASE_SIMPLE_TEST_CLOCK_H_
