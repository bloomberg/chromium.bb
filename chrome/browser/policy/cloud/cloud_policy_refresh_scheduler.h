// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_H_
#define CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_H_

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/policy/cloud/rate_limiter.h"
#include "net/base/network_change_notifier.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

// Observes CloudPolicyClient and CloudPolicyStore to trigger periodic policy
// fetches and issue retries on error conditions.
class CloudPolicyRefreshScheduler
    : public CloudPolicyClient::Observer,
      public CloudPolicyStore::Observer,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // Refresh constants.
  static const int64 kDefaultRefreshDelayMs;
  static const int64 kUnmanagedRefreshDelayMs;
  static const int64 kWithInvalidationsRefreshDelayMs;
  static const int64 kInitialErrorRetryDelayMs;

  // Refresh delay bounds.
  static const int64 kRefreshDelayMinMs;
  static const int64 kRefreshDelayMaxMs;

  // |client|, |store| and |prefs| pointers must stay valid throughout the
  // lifetime of CloudPolicyRefreshScheduler.
  CloudPolicyRefreshScheduler(
      CloudPolicyClient* client,
      CloudPolicyStore* store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  virtual ~CloudPolicyRefreshScheduler();

  base::Time last_refresh() const { return last_refresh_; }
  int64 refresh_delay() const { return refresh_delay_ms_; }

  // Sets the refresh delay to |refresh_delay| (subject to min/max clamping).
  void SetRefreshDelay(int64 refresh_delay);

  // Requests a policy refresh to be performed soon. This may apply throttling,
  // and the request may not be immediately sent.
  void RefreshSoon();

  // The refresh scheduler starts by assuming that invalidations are not
  // available. This call can be used to signal whether the invalidations
  // service is available or not, and can be called multiple times.
  // When the invalidations service is available then the refresh rate is much
  // lower.
  void SetInvalidationServiceAvailability(bool is_available);

  // Whether the invalidations service is available and receiving notifications
  // of policy updates.
  bool invalidations_available() {
    return invalidations_available_;
  }

  // CloudPolicyClient::Observer:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

  // net::NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

 private:
  // Initializes |last_refresh_| to the policy timestamp from |store_| in case
  // there is policy present that indicates this client is not managed. This
  // results in policy fetches only to occur after the entire unmanaged refresh
  // delay expires, even over restarts. For managed clients, we want to trigger
  // a refresh on every restart.
  void UpdateLastRefreshFromPolicy();

  // Schedules a refresh to be performed immediately.
  void RefreshNow();

  // Evaluates when the next refresh is pending and updates the callback to
  // execute that refresh at the appropriate time.
  void ScheduleRefresh();

  // Triggers a policy refresh.
  void PerformRefresh();

  // Schedules a policy refresh to happen after |delta_ms| milliseconds,
  // relative to |last_refresh_|.
  void RefreshAfter(int delta_ms);

  // Sets the |wait_for_invalidations_timeout_callback_| and schedules it.
  void WaitForInvalidationService();

  // Callback for |wait_for_invalidations_timeout_callback_|.
  void OnWaitForInvalidationServiceTimeout();

  // Returns true if the refresh scheduler is currently waiting for the
  // availability of the invalidations service.
  bool WaitingForInvalidationService() const;

  CloudPolicyClient* client_;
  CloudPolicyStore* store_;

  // For scheduling delayed tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The delayed refresh callback.
  base::CancelableClosure refresh_callback_;

  // The last time a refresh callback completed.
  base::Time last_refresh_;

  // Error retry delay in milliseconds.
  int64 error_retry_delay_ms_;

  // The refresh delay.
  int64 refresh_delay_ms_;

  // Used to limit the rate at which refreshes are scheduled.
  RateLimiter rate_limiter_;

  // Whether the invalidations service is available and receiving notifications
  // of policy updates.
  bool invalidations_available_;

  // The refresh scheduler waits some seconds for the invalidations service
  // before starting to issue refresh requests. If the invalidations service
  // doesn't become available during this time then the refresh scheduler will
  // use the polling refresh rate.
  base::CancelableClosure wait_for_invalidations_timeout_callback_;

  // Used to measure how long it took for the invalidations service to report
  // its initial status.
  base::Time creation_time_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyRefreshScheduler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_H_
