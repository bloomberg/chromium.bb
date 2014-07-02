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
class DevToolsNetworkTransaction;

namespace base {
class TimeDelta;
class TimeTicks;
}

// DevToolsNetworkInterceptor emulates network conditions for transactions with
// specific client id.
class DevToolsNetworkInterceptor {

 public:
  DevToolsNetworkInterceptor();
  virtual ~DevToolsNetworkInterceptor();

  base::WeakPtr<DevToolsNetworkInterceptor> GetWeakPtr();

  // Applies network emulation configuration.
  void UpdateConditions(scoped_ptr<DevToolsNetworkConditions> conditions);

  void AddTransaction(DevToolsNetworkTransaction* transaction);
  void RemoveTransaction(DevToolsNetworkTransaction* transaction);

  bool ShouldFail(const DevToolsNetworkTransaction* transaction);
  bool ShouldThrottle(const DevToolsNetworkTransaction* transaction);
  void ThrottleTransaction(DevToolsNetworkTransaction* transaction, bool start);

  const DevToolsNetworkConditions* conditions() const {
    return conditions_.get();
  }

 private:
  scoped_ptr<DevToolsNetworkConditions> conditions_;

  void UpdateThrottledTransactions(base::TimeTicks now);
  void UpdateSuspendedTransactions(base::TimeTicks now);
  void ArmTimer(base::TimeTicks now);
  void OnTimer();

  void FireThrottledCallback(DevToolsNetworkTransaction* transaction);

  typedef std::set<DevToolsNetworkTransaction*> Transactions;
  Transactions transactions_;

  // Transactions suspended for a "latency" period.
  typedef std::pair<DevToolsNetworkTransaction*, int64_t> SuspendedTransaction;
  typedef std::vector<SuspendedTransaction> SuspendedTransactions;
  SuspendedTransactions suspended_transactions_;

  // Transactions waiting certain amount of transfer to be "accounted".
  std::vector<DevToolsNetworkTransaction*> throttled_transactions_;

  base::OneShotTimer<DevToolsNetworkInterceptor> timer_;
  base::TimeTicks offset_;
  base::TimeDelta tick_length_;
  base::TimeDelta latency_length_;
  uint64_t last_tick_;

  base::WeakPtrFactory<DevToolsNetworkInterceptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkInterceptor);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_INTERCEPTOR_H_
