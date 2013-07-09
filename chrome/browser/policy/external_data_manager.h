// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_EXTERNAL_DATA_MANAGER_H_
#define CHROME_BROWSER_POLICY_EXTERNAL_DATA_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/policy/external_data_fetcher.h"

namespace policy {

// Downloads, verifies, caches and retrieves external data referenced by
// policies.
// TODO(bartfab): Implement support for fetching external policy data.
// http://crbug.com/256635
class ExternalDataManager {
 public:
  void Fetch(const std::string& policy,
             const ExternalDataFetcher::FetchCallback& callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalDataManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_EXTERNAL_DATA_MANAGER_H_
