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
  using ThrottleCallback = base::Callback<void(int, int64_t)>;

  DevToolsNetworkInterceptor();
  virtual ~DevToolsNetworkInterceptor();

  base::WeakPtr<DevToolsNetworkInterceptor> GetWeakPtr();

  // Applies network emulation configuration.
  void UpdateConditions(scoped_ptr<DevToolsNetworkConditions> conditions);

  int StartThrottle(int result,
                    int64_t bytes,
                    base::TimeTicks send_end,
                    bool start,
                    const ThrottleCallback& callback);
  void StopThrottle(const ThrottleCallback& callback);

  bool IsOffline();

 private:
  scoped_ptr<DevToolsNetworkConditions> conditions_;

  void UpdateThrottled(base::TimeTicks now);
  void UpdateSuspended(base::TimeTicks now);
  void ArmTimer(base::TimeTicks now);
  void OnTimer();

  struct ThrottleRecord {
   public:
    ThrottleRecord();
    ~ThrottleRecord();
    int result;
    int64_t bytes;
    int64_t send_end;
    ThrottleCallback callback;
  };
  using ThrottleRecords = std::vector<ThrottleRecord>;

  void RemoveRecord(ThrottleRecords* records, const ThrottleCallback& callback);

  // Throttables suspended for a "latency" period.
  ThrottleRecords suspended_;

  // Throttable waiting certain amount of transfer to be "accounted".
  ThrottleRecords throttled_;

  base::OneShotTimer timer_;
  base::TimeTicks offset_;
  base::TimeDelta tick_length_;
  base::TimeDelta latency_length_;
  uint64_t last_tick_;

  base::WeakPtrFactory<DevToolsNetworkInterceptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkInterceptor);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_INTERCEPTOR_H_
