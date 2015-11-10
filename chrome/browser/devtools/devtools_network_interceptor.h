// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_INTERCEPTOR_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_INTERCEPTOR_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"

class DevToolsNetworkConditions;

namespace base {
class TimeDelta;
class TimeTicks;
}

// DevToolsNetworkInterceptor emulates network conditions for transactions with
// specific client id.
class DevToolsNetworkInterceptor {
 public:
  class Throttable {
   public:
    virtual ~Throttable() {}
    virtual void Fail() = 0;
    virtual int64_t ThrottledByteCount() = 0;
    virtual void Throttled(int64_t count) = 0;
    virtual void ThrottleFinished() = 0;
    virtual void GetSendEndTiming(base::TimeTicks* send_end) = 0;
  };

  DevToolsNetworkInterceptor();
  virtual ~DevToolsNetworkInterceptor();

  base::WeakPtr<DevToolsNetworkInterceptor> GetWeakPtr();

  // Applies network emulation configuration.
  void UpdateConditions(scoped_ptr<DevToolsNetworkConditions> conditions);

  void AddThrottable(Throttable* throttable);
  void RemoveThrottable(Throttable* throttable);

  bool ShouldFail();
  bool ShouldThrottle();
  void Throttle(Throttable* throttable, bool start);

  const DevToolsNetworkConditions* conditions() const {
    return conditions_.get();
  }

 private:
  scoped_ptr<DevToolsNetworkConditions> conditions_;

  void UpdateThrottled(base::TimeTicks now);
  void UpdateSuspended(base::TimeTicks now);
  void ArmTimer(base::TimeTicks now);
  void OnTimer();

  void FireThrottledCallback(Throttable* throttable);

  typedef std::set<Throttable*> Throttables;
  Throttables throttables_;

  // Throttables suspended for a "latency" period.
  typedef std::vector<std::pair<Throttable*, int64_t>> Suspended;
  Suspended suspended_;

  // Throttable waiting certain amount of transfer to be "accounted".
  std::vector<Throttable*> throttled_;

  base::OneShotTimer timer_;
  base::TimeTicks offset_;
  base::TimeDelta tick_length_;
  base::TimeDelta latency_length_;
  uint64_t last_tick_;

  base::WeakPtrFactory<DevToolsNetworkInterceptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkInterceptor);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_INTERCEPTOR_H_
