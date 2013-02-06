// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_STATISTICS_COLLECTOR_H_
#define CHROME_BROWSER_POLICY_POLICY_STATISTICS_COLLECTOR_H_

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class TaskRunner;
}

namespace policy {

class PolicyService;

// Manages regular updates of policy usage UMA histograms.
class PolicyStatisticsCollector {
 public:
  // Policy usage statistics update rate, in milliseconds.
  static const int kStatisticsUpdateRate;

  // Neither |policy_service| nor |prefs| can be NULL and must stay valid
  // throughout the lifetime of PolicyStatisticsCollector.
  PolicyStatisticsCollector(PolicyService* policy_service,
                            PrefService* prefs,
                            const scoped_refptr<base::TaskRunner>& task_runner);
  virtual ~PolicyStatisticsCollector();

  // Completes initialization and starts periodical statistic updates.
  void Initialize();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // protected virtual for mocking.
  virtual void RecordPolicyUse(int id);

 private:
  void CollectStatistics();
  void ScheduleUpdate(base::TimeDelta delay);

  int max_policy_id_;

  PolicyService* policy_service_;
  PrefService* prefs_;

  base::CancelableClosure update_callback_;

  const scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PolicyStatisticsCollector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_STATISTICS_COLLECTOR_H_
