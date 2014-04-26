// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SCHEDULER_TEST_COMMON_H_
#define CC_TEST_SCHEDULER_TEST_COMMON_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FakeTimeSourceClient : public TimeSourceClient {
 public:
  FakeTimeSourceClient() { Reset(); }
  void Reset() { tick_called_ = false; }
  bool TickCalled() const { return tick_called_; }

  // TimeSourceClient implementation.
  virtual void OnTimerTick() OVERRIDE;

 protected:
  bool tick_called_;
};

class FakeDelayBasedTimeSource : public DelayBasedTimeSource {
 public:
  static scoped_refptr<FakeDelayBasedTimeSource> Create(
      base::TimeDelta interval, base::SingleThreadTaskRunner* task_runner) {
    return make_scoped_refptr(new FakeDelayBasedTimeSource(interval,
                                                           task_runner));
  }

  void SetNow(base::TimeTicks time) { now_ = time; }
  virtual base::TimeTicks Now() const OVERRIDE;

 protected:
  FakeDelayBasedTimeSource(base::TimeDelta interval,
                           base::SingleThreadTaskRunner* task_runner)
      : DelayBasedTimeSource(interval, task_runner) {}
  virtual ~FakeDelayBasedTimeSource() {}

  base::TimeTicks now_;
};

}  // namespace cc

#endif  // CC_TEST_SCHEDULER_TEST_COMMON_H_
