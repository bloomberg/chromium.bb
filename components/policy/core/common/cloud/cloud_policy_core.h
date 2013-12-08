// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_member.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/policy_export.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CloudPolicyClient;
class CloudPolicyRefreshScheduler;
class CloudPolicyService;
class CloudPolicyStore;

// CloudPolicyCore glues together the ingredients that are essential for
// obtaining a fully-functional cloud policy system: CloudPolicyClient and
// CloudPolicyStore, which are responsible for fetching policy from the cloud
// and storing it locally, respectively, as well as a CloudPolicyService
// instance that moves data between the two former components, and
// CloudPolicyRefreshScheduler which triggers periodic refreshes.
class POLICY_EXPORT CloudPolicyCore {
 public:
  // Callbacks for policy core events.
  class POLICY_EXPORT Observer {
   public:
    virtual ~Observer();

    // Called after the core is connected.
    virtual void OnCoreConnected(CloudPolicyCore* core) = 0;

    // Called after the refresh scheduler is started.
    virtual void OnRefreshSchedulerStarted(CloudPolicyCore* core) = 0;

    // Called before the core is disconnected.
    virtual void OnCoreDisconnecting(CloudPolicyCore* core) = 0;
  };

  // |task_runner| is the runner for policy refresh tasks.
  CloudPolicyCore(const PolicyNamespaceKey& policy_ns_key,
                  CloudPolicyStore* store,
                  const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~CloudPolicyCore();

  CloudPolicyClient* client() { return client_.get(); }
  const CloudPolicyClient* client() const { return client_.get(); }

  CloudPolicyStore* store() { return store_; }
  const CloudPolicyStore* store() const { return store_; }

  CloudPolicyService* service() { return service_.get(); }
  const CloudPolicyService* service() const { return service_.get(); }

  CloudPolicyRefreshScheduler* refresh_scheduler() {
    return refresh_scheduler_.get();
  }
  const CloudPolicyRefreshScheduler* refresh_scheduler() const {
    return refresh_scheduler_.get();
  }

  // Initializes the cloud connection.
  void Connect(scoped_ptr<CloudPolicyClient> client);

  // Shuts down the cloud connection.
  void Disconnect();

  // Requests a policy refresh to be performed soon. This may apply throttling,
  // and the request may not be immediately sent.
  void RefreshSoon();

  // Starts a refresh scheduler in case none is running yet.
  void StartRefreshScheduler();

  // Watches the pref named |refresh_pref_name| in |pref_service| and adjusts
  // |refresh_scheduler_|'s refresh delay accordingly.
  void TrackRefreshDelayPref(PrefService* pref_service,
                             const std::string& refresh_pref_name);

  // Registers an observer to be notified of policy core events.
  void AddObserver(Observer* observer);

  // Removes the specified observer.
  void RemoveObserver(Observer* observer);

 private:
  // Updates the refresh scheduler on refresh delay changes.
  void UpdateRefreshDelayFromPref();

  PolicyNamespaceKey policy_ns_key_;
  CloudPolicyStore* store_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<CloudPolicyClient> client_;
  scoped_ptr<CloudPolicyService> service_;
  scoped_ptr<CloudPolicyRefreshScheduler> refresh_scheduler_;
  scoped_ptr<IntegerPrefMember> refresh_delay_;
  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyCore);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CORE_H_
