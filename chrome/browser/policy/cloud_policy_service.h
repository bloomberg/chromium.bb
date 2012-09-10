// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_SERVICE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_store.h"

namespace policy {

// Coordinates cloud policy handling, moving downloaded policy from the client
// to the store, and setting up client registrations from cached data in the
// store. Also coordinates actions on policy refresh triggers.
class CloudPolicyService : public CloudPolicyClient::Observer,
                           public CloudPolicyStore::Observer {
 public:
  // |client| and |store| must remain valid for the object life time.
  CloudPolicyService(CloudPolicyClient* client, CloudPolicyStore* store);
  virtual ~CloudPolicyService();

  // Returns the domain that manages this user/device, according to the current
  // policy blob. Empty if not managed/not available.
  std::string ManagedBy() const;

  // Refreshes policy. |callback| will be invoked after the operation completes
  // or aborts because of errors.
  void RefreshPolicy(const base::Closure& callback);

  // CloudPolicyClient::Observer:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 private:
  // Invokes the refresh callbacks and clears refresh state.
  void RefreshCompleted();

  // The client used to talk to the cloud.
  CloudPolicyClient* client_;

  // Takes care of persisting and decoding cloud policy.
  CloudPolicyStore* store_;

  // Tracks the state of a pending refresh operation, if any.
  enum {
    // No refresh pending.
    REFRESH_NONE,
    // Policy fetch is pending.
    REFRESH_POLICY_FETCH,
    // Policy store is pending.
    REFRESH_POLICY_STORE,
  } refresh_state_;

  // Callbacks to invoke upon policy refresh.
  std::vector<base::Closure> refresh_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_SERVICE_H_
