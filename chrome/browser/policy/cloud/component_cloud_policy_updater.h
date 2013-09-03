// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_UPDATER_H_
#define CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_UPDATER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud/external_policy_data_updater.h"

namespace base {
class SequencedTaskRunner;
}

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class ComponentCloudPolicyStore;

// This class downloads external policy data, given PolicyFetchResponses.
// It validates the PolicyFetchResponse and its corresponding data, and caches
// them in a ComponentCloudPolicyStore. It also enforces size limits on what's
// cached.
// It retries to download the policy data periodically when a download fails.
class ComponentCloudPolicyUpdater {
 public:
  // |task_runner| must support file I/O, and is used to post delayed retry
  // tasks.
  // |request_context| will be used for the download fetchers.
  // |store| must outlive the updater, and is where the downloaded data will
  // be cached.
  ComponentCloudPolicyUpdater(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<net::URLRequestContextGetter> request_context,
      ComponentCloudPolicyStore* store);
  ~ComponentCloudPolicyUpdater();

  // |response| is the latest policy information fetched for some component.
  // This method schedules the download of the policy data, if |response| is
  // validated. If the downloaded data also passes validation then that data
  // will be passed to the |store_|.
  void UpdateExternalPolicy(
      scoped_ptr<enterprise_management::PolicyFetchResponse> response);

 private:
  ComponentCloudPolicyStore* store_;
  ExternalPolicyDataUpdater external_policy_data_updater_;

  DISALLOW_COPY_AND_ASSIGN(ComponentCloudPolicyUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_UPDATER_H_
