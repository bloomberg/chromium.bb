// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"

class DevToolsNetworkConditions;
class DevToolsNetworkTransaction;
class GURL;
class Profile;

namespace base {
class TimeDelta;
class TimeTicks;
}

namespace content {
class ResourceContext;
}

namespace net {
struct HttpRequestInfo;
}

namespace test {
class DevToolsNetworkControllerHelper;
}

// DevToolsNetworkController tracks DevToolsNetworkTransactions.
class DevToolsNetworkController {

 public:
  DevToolsNetworkController();
  virtual ~DevToolsNetworkController();

  void AddTransaction(DevToolsNetworkTransaction* transaction);

  void RemoveTransaction(DevToolsNetworkTransaction* transaction);

  // Applies network emulation configuration.
  void SetNetworkState(
      const scoped_refptr<DevToolsNetworkConditions> conditions);

  bool ShouldFail(const net::HttpRequestInfo* request);
  bool ShouldThrottle(const net::HttpRequestInfo* request);
  void ThrottleTransaction(DevToolsNetworkTransaction* transaction);

 protected:
  friend class test::DevToolsNetworkControllerHelper;

 private:
  // Controller must be constructed on IO thread.
  base::ThreadChecker thread_checker_;

  typedef scoped_refptr<DevToolsNetworkConditions> Conditions;

  void SetNetworkStateOnIO(const Conditions conditions);

  typedef std::set<DevToolsNetworkTransaction*> Transactions;
  Transactions transactions_;

  Conditions conditions_;

  void UpdateThrottles();
  void ArmTimer();
  void OnTimer();

  std::vector<DevToolsNetworkTransaction*> throttled_transactions_;
  base::OneShotTimer<DevToolsNetworkController> timer_;
  base::TimeTicks offset_;
  base::TimeDelta tick_length_;
  uint64_t last_tick_;

  base::WeakPtrFactory<DevToolsNetworkController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkController);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
