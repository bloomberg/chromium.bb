// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_EXTERNAL_DATA_MANAGER_H_
#define CHROME_BROWSER_POLICY_EXTERNAL_DATA_MANAGER_H_

#include <string>

#include "chrome/browser/policy/external_data_fetcher.h"

namespace policy {

// Downloads, verifies, caches and retrieves external data referenced by
// policies.
// An implementation of this abstract interface should be provided for each
// policy source (cloud policy, platform policy) that supports external data
// references.
class ExternalDataManager {
 public:
  virtual void Fetch(const std::string& policy,
                     const ExternalDataFetcher::FetchCallback& callback) = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_EXTERNAL_DATA_MANAGER_H_
