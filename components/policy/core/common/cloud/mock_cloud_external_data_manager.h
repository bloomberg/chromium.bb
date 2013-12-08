// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_EXTERNAL_DATA_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_EXTERNAL_DATA_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class ExternalDataFetcher;

class MockCloudExternalDataManager : public CloudExternalDataManager {
 public:
  MockCloudExternalDataManager();
  virtual ~MockCloudExternalDataManager();

  MOCK_METHOD0(OnPolicyStoreLoaded, void(void));
  MOCK_METHOD1(Connect, void(scoped_refptr<net::URLRequestContextGetter>));
  MOCK_METHOD0(Disconnect, void(void));
  MOCK_METHOD2(Fetch, void(const std::string&,
                           const ExternalDataFetcher::FetchCallback&));

  scoped_ptr<ExternalDataFetcher> CreateExternalDataFetcher(
      const std::string& policy);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudExternalDataManager);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_EXTERNAL_DATA_MANAGER_H_
