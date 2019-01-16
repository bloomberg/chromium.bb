// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_EXTERNAL_DATA_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_EXTERNAL_DATA_MANAGER_H_

#include <string>

#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/policy_export.h"

namespace policy {

// Downloads, verifies, caches and retrieves external data referenced by
// policies.
// An implementation of this abstract interface should be provided for each
// policy source (cloud policy, platform policy) that supports external data
// references.
class POLICY_EXPORT ExternalDataManager {
 public:
  // Retrieves the external data referenced by |policy| and invokes |callback|
  // with the result. If |policy| does not reference any external data, the
  // |callback| is invoked with a NULL pointer. Otherwise, the |callback| is
  // invoked with the referenced data once it has been successfully retrieved.
  // If retrieval is temporarily impossible (e.g. no network connectivity), the
  // |callback| will be invoked when the temporary hindrance is resolved. If
  // retrieval is permanently impossible (e.g. |policy| references data that
  // does not exist on the server), the |callback| will never be invoked.
  virtual void Fetch(const std::string& policy,
                     ExternalDataFetcher::FetchCallback callback) = 0;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_EXTERNAL_DATA_MANAGER_H_
