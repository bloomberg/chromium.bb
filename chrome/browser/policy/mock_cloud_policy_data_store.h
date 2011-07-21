// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CLOUD_POLICY_DATA_STORE_H_
#define CHROME_BROWSER_POLICY_MOCK_CLOUD_POLICY_DATA_STORE_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockCloudPolicyDataStoreObserver : public CloudPolicyDataStore::Observer {
 public:
  MockCloudPolicyDataStoreObserver();
  virtual ~MockCloudPolicyDataStoreObserver();

  MOCK_METHOD0(OnDeviceTokenChanged, void());
  MOCK_METHOD0(OnCredentialsChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyDataStoreObserver);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_CLOUD_POLICY_DATA_STORE_H_
