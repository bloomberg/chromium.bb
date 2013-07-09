// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_EXTERNAL_DATA_FETCHER_H_
#define CHROME_BROWSER_POLICY_EXTERNAL_DATA_FETCHER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

namespace policy {

class ExternalDataManager;

// A helper that encapsulates the parameters required to retrieve the external
// data for a policy.
class ExternalDataFetcher {
 public:
  enum Status {
    // This policy does not reference any external data.
    STATUS_NO_DATA,
    // This policy references external data. The data has been requested but is
    // not available yet. Once the data is successfully downloaded and verified,
    // the PolicyService that this ExternalDataFetcher belongs to will invoke
    // OnPolicyUpdated() to inform its observers that the data is available.
    STATUS_DATA_PENDING,
    // This external data referenced by the policy has been downloaded, verified
    // and is included in the callback.
    STATUS_DATA_AVAILABLE
  };

  typedef base::Callback<void(Status, scoped_ptr<std::string>)> FetchCallback;

  ExternalDataFetcher(const ExternalDataFetcher& other);
  ~ExternalDataFetcher();

  static bool Equals(const ExternalDataFetcher* first,
                     const ExternalDataFetcher* second);

  void Fetch(const FetchCallback& callback) const;

 protected:
  friend class PolicyDomainDescriptorTest;
  friend class PolicyMapTest;

  ExternalDataFetcher(base::WeakPtr<ExternalDataManager> manager,
                      const std::string& policy);

 private:
  base::WeakPtr<ExternalDataManager> manager_;
  std::string policy_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_EXTERNAL_DATA_FETCHER_H_
