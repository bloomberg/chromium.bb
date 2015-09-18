// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace test {

void ExternalDataFetchCallback(scoped_ptr<std::string>* destination,
                               const base::Closure& done_callback,
                               scoped_ptr<std::string> data) {
  *destination = data.Pass();
  done_callback.Run();
}

scoped_ptr<base::DictionaryValue> ConstructExternalDataReference(
    const std::string& url,
    const std::string& data) {
  const std::string hash = crypto::SHA256HashString(data);
  scoped_ptr<base::DictionaryValue> metadata(new base::DictionaryValue);
  metadata->SetStringWithoutPathExpansion("url", url);
  metadata->SetStringWithoutPathExpansion("hash", base::HexEncode(hash.c_str(),
                                                                  hash.size()));
  return metadata.Pass();
}

void SetExternalDataReference(CloudPolicyCore* core,
                              const std::string& policy,
                              scoped_ptr<base::DictionaryValue> metadata) {
  CloudPolicyStore* store = core->store();
  ASSERT_TRUE(store);
  PolicyMap policy_map;
  policy_map.Set(policy,
                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 metadata.release(),
                 new ExternalDataFetcher(store->external_data_manager(),
                                         policy));
  store->SetPolicyMapForTesting(policy_map);
}

}  // namespace test
}  // namespace policy
