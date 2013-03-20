// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_TIME_SOURCE_H_
#define CC_SCHEDULER_TIME_SOURCE_H_

#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cc/base/cc_export.h"

namespace cc {

class TimeSourceClient {
 public:
  virtual void OnTimerTick() = 0;

 protected:
  virtual ~TimeSourceClient() {}
};

// An generic interface for getting a reliably-ticking timesource of
// a specified rate.
//
// Be sure to call SetActive(false) before releasing your reference to the
// timer, or it will keep on ticking!
class CC_EXPORT TimeSource : public base::RefCounted<TimeSource> {
 public:
  virtual void SetClient(TimeSourceClient* client) = 0;
  virtual void SetActive(bool active) = 0;
  virtual bool Active() const = 0;
  virtual void SetTimebaseAndInterval(base::TimeTicks timebase,
                                      base::TimeDelta interval) = 0;
  virtual base::TimeTicks LastTickTime() = 0;
  virtual base::TimeTicks NextTickTime() = 0;

 protected:
  virtual ~TimeSource() {}

 private:
  friend class base::RefCounted<TimeSource>;
};

}  // namespace cc

#endif  // CC_SCHEDULER_TIME_SOURCE_H_
