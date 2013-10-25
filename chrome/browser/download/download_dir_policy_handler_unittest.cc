// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/download/download_dir_policy_handler.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_pref_store_unittest.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/common/pref_names.h"
#include "policy/policy_constants.h"

class DownloadDirPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
 public:
  virtual void SetUp() OVERRIDE {
    handler_list_.AddHandler(
        make_scoped_ptr<policy::ConfigurationPolicyHandler>(
            new DownloadDirPolicyHandler));
  }
};

TEST_F(DownloadDirPolicyHandlerTest, SetDownloadDirectory) {
  policy::PolicyMap policy;
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
  policy.Set(policy::key::kDownloadDirectory,
             policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             base::Value::CreateStringValue(std::string()),
             NULL);
  UpdateProviderPolicy(policy);

  // Setting a DownloadDirectory should disable the PromptForDownload pref.
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kPromptForDownload, &value));
  ASSERT_TRUE(value);
  bool prompt_for_download = true;
  bool result = value->GetAsBoolean(&prompt_for_download);
  ASSERT_TRUE(result);
  EXPECT_FALSE(prompt_for_download);
}
