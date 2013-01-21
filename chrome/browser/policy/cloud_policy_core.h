// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_CORE_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_CORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/policy/cloud_policy_constants.h"

class PrefService;

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
class CloudPolicyCore {
 public:
  CloudPolicyCore(const PolicyNamespaceKey& policy_ns_key,
                  CloudPolicyStore* store);
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

  // Starts a refresh scheduler in case none is running yet.
  void StartRefreshScheduler();

  // Watches the pref named |refresh_pref_name| in |pref_service| and adjusts
  // |refresh_scheduler_|'s refresh delay accordingly.
  void TrackRefreshDelayPref(PrefService* pref_service,
                             const std::string& refresh_pref_name);

 private:
  // Updates the refresh scheduler on refresh delay changes.
  void UpdateRefreshDelayFromPref();

  PolicyNamespaceKey policy_ns_key_;
  CloudPolicyStore* store_;
  scoped_ptr<CloudPolicyClient> client_;
  scoped_ptr<CloudPolicyService> service_;
  scoped_ptr<CloudPolicyRefreshScheduler> refresh_scheduler_;
  scoped_ptr<IntegerPrefMember> refresh_delay_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyCore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_CORE_H_
