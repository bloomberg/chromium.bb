// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "components/leveldb_proto/internal/leveldb_proto_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leveldb_proto {

namespace {
const char kTestClientName[] = "TestClientName";
}

class SharedProtoDatabaseClientListTest : public testing::Test {
 public:
  void SetUpExperimentParam(std::string key, std::string value) {
    std::map<std::string, std::string> params = {
        {"migrate_ClientNameFoo", "true"},
        {"migrate_" + key, value},
        {"migrate_ClientNameBar", "false"},
    };

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kProtoDBSharedMigration, params);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SharedProtoDatabaseClientListTest, ShouldUseSharedDBTest) {
  // Parameter value is case sensitive
  SetUpExperimentParam(kTestClientName, "true");

  bool use_shared =
      SharedProtoDatabaseClientList::ShouldUseSharedDB(kTestClientName);

  ASSERT_TRUE(use_shared);
}

TEST_F(SharedProtoDatabaseClientListTest,
       ShouldUseSharedDBTest_OnlyWhenParamMatchesName) {
  SetUpExperimentParam("AnotherClientName", "true");

  bool use_shared =
      SharedProtoDatabaseClientList::ShouldUseSharedDB(kTestClientName);

  ASSERT_FALSE(use_shared);
}

TEST_F(SharedProtoDatabaseClientListTest,
       ShouldUseSharedDBTest_OnlyWhenParamValueIsTrue) {
  SetUpExperimentParam(kTestClientName, "false");

  bool use_shared =
      SharedProtoDatabaseClientList::ShouldUseSharedDB(kTestClientName);

  ASSERT_FALSE(use_shared);
}

}  // namespace leveldb_proto
