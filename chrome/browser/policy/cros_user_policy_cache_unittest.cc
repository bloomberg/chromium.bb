// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/time.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/cros_user_policy_cache.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "content/public/test/test_browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeArgument;
using testing::_;

namespace em = enterprise_management;

// These should probably be moved to a more central place.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p1)) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(::std::tr1::get<k>(args), p1));
}
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p1, p2)) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(::std::tr1::get<k>(args), p1, p2));
}

namespace policy {
namespace {

const char kFakeToken[] = "token";
const char kFakeSignature[] = "signature";
const char kFakeMachineName[] = "machine-name";
const char kFakeUsername[] = "username";
const int kFakePublicKeyVersion = 17;
const char kFakeDeviceId[] = "device-id";

class CrosUserPolicyCacheTest : public testing::Test {
 protected:
  CrosUserPolicyCacheTest()
      : loop_(MessageLoop::TYPE_UI),
        data_store_(CloudPolicyDataStore::CreateForUserPolicies()),
        ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_) {}

  virtual void SetUp() {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
  }

  void CreatePolicyResponse(em::PolicyFetchResponse* response) {
    em::CloudPolicySettings policy_settings;
    policy_settings.mutable_showhomebutton()->set_showhomebutton(true);

    em::PolicyData policy_data;
    policy_data.set_policy_type(dm_protocol::kChromeUserPolicyType);
    policy_data.set_timestamp((base::Time::NowFromSystemTime() -
                               base::Time::UnixEpoch()).InMilliseconds());
    policy_data.set_request_token(kFakeToken);
    ASSERT_TRUE(
        policy_settings.SerializeToString(policy_data.mutable_policy_value()));
    policy_data.set_machine_name(kFakeMachineName);
    policy_data.set_public_key_version(kFakePublicKeyVersion);
    policy_data.set_username(kFakeUsername);
    policy_data.set_device_id(kFakeDeviceId);
    policy_data.set_state(em::PolicyData::ACTIVE);

    ASSERT_TRUE(policy_data.SerializeToString(response->mutable_policy_data()));
    response->set_policy_data_signature(kFakeSignature);
  }

  FilePath token_file() {
    return tmp_dir_.path().Append(FILE_PATH_LITERAL("token"));
  }

  FilePath policy_file() {
    return tmp_dir_.path().Append(FILE_PATH_LITERAL("policy"));
  }

  MessageLoop loop_;

  chromeos::MockSessionManagerClient session_manager_client_;
  scoped_ptr<CloudPolicyDataStore> data_store_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  ScopedTempDir tmp_dir_;

  DISALLOW_COPY_AND_ASSIGN(CrosUserPolicyCacheTest);
};

TEST_F(CrosUserPolicyCacheTest, InitAndSetPolicy) {
  CrosUserPolicyCache cache(&session_manager_client_,
                            data_store_.get(),
                            false,
                            token_file(),
                            policy_file());
  EXPECT_FALSE(cache.IsReady());
  EXPECT_FALSE(data_store_->token_cache_loaded());

  // Initialize the cache.
  EXPECT_CALL(session_manager_client_, RetrieveUserPolicy(_))
      .WillOnce(InvokeCallbackArgument<0>(std::string()));

  cache.Load();
  loop_.RunAllPending();

  EXPECT_TRUE(cache.IsReady());
  EXPECT_TRUE(data_store_->token_cache_loaded());
  EXPECT_EQ(0U, cache.policy()->size());

  // Set policy.
  em::PolicyFetchResponse response;
  CreatePolicyResponse(&response);
  std::string serialized_response;
  ASSERT_TRUE(response.SerializeToString(&serialized_response));
  testing::Sequence seq;
  EXPECT_CALL(session_manager_client_, StoreUserPolicy(serialized_response, _))
      .InSequence(seq)
      .WillOnce(InvokeCallbackArgument<1>(true));
  EXPECT_CALL(session_manager_client_, RetrieveUserPolicy(_))
      .InSequence(seq)
      .WillOnce(InvokeCallbackArgument<0>(serialized_response));

  EXPECT_TRUE(cache.SetPolicy(response));
  loop_.RunAllPending();

  EXPECT_EQ(1U, cache.policy()->size());
  const PolicyMap::Entry* entry = cache.policy()->Get(key::kShowHomeButton);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(base::FundamentalValue(true).Equals(entry->value));
};

TEST_F(CrosUserPolicyCacheTest, Migration) {
  std::string data;

  em::DeviceCredentials credentials;
  credentials.set_device_token(kFakeToken);
  credentials.set_device_id(kFakeDeviceId);
  ASSERT_TRUE(credentials.SerializeToString(&data));
  ASSERT_NE(-1, file_util::WriteFile(token_file(), data.c_str(), data.size()));

  em::CachedCloudPolicyResponse policy;
  CreatePolicyResponse(policy.mutable_cloud_policy());
  ASSERT_TRUE(policy.SerializeToString(&data));
  ASSERT_NE(-1, file_util::WriteFile(policy_file(), data.c_str(), data.size()));

  CrosUserPolicyCache cache(&session_manager_client_,
                            data_store_.get(),
                            false,
                            token_file(),
                            policy_file());
  EXPECT_FALSE(cache.IsReady());
  EXPECT_FALSE(data_store_->token_cache_loaded());

  // Initialize the cache.
  EXPECT_CALL(session_manager_client_, RetrieveUserPolicy(_))
      .WillOnce(InvokeCallbackArgument<0>(std::string()));

  cache.Load();
  loop_.RunAllPending();

  EXPECT_TRUE(cache.IsReady());
  EXPECT_TRUE(data_store_->token_cache_loaded());
  EXPECT_EQ(kFakeDeviceId, data_store_->device_id());
  EXPECT_EQ(kFakeToken, data_store_->device_token());
  EXPECT_EQ(1U, cache.policy()->size());
  const PolicyMap::Entry* entry = cache.policy()->Get(key::kShowHomeButton);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(base::FundamentalValue(true).Equals(entry->value));
};

}  // namespace
}  // namespace policy
