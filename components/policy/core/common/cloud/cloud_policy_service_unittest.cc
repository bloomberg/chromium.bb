// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::_;

namespace policy {

class MockCloudPolicyServiceObserver : public CloudPolicyService::Observer {
 public:
  MockCloudPolicyServiceObserver() {}
  virtual ~MockCloudPolicyServiceObserver() {}

  MOCK_METHOD1(OnInitializationCompleted, void(CloudPolicyService* service));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyServiceObserver);
};

class CloudPolicyServiceTest : public testing::Test {
 public:
  CloudPolicyServiceTest()
      : policy_ns_key_(dm_protocol::kChromeUserPolicyType, std::string()),
        service_(policy_ns_key_, &client_, &store_) {}

  MOCK_METHOD1(OnPolicyRefresh, void(bool));

 protected:
  PolicyNamespaceKey policy_ns_key_;
  MockCloudPolicyClient client_;
  MockCloudPolicyStore store_;
  CloudPolicyService service_;
};

MATCHER_P(ProtoMatches, proto, "") {
  return arg.SerializePartialAsString() == proto.SerializePartialAsString();
}

TEST_F(CloudPolicyServiceTest, ManagedByEmptyPolicy) {
  EXPECT_EQ("", service_.ManagedBy());
}

TEST_F(CloudPolicyServiceTest, ManagedByValidPolicy) {
  store_.policy_.reset(new em::PolicyData());
  store_.policy_->set_username("user@example.com");
  EXPECT_EQ("example.com", service_.ManagedBy());
}

TEST_F(CloudPolicyServiceTest, PolicyUpdateSuccess) {
  em::PolicyFetchResponse policy;
  policy.set_policy_data("fake policy");
  client_.SetPolicy(policy_ns_key_, policy);
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  client_.NotifyPolicyFetched();

  // After |store_| initializes, credentials and other meta data should be
  // transferred to |client_|.
  store_.policy_.reset(new em::PolicyData());
  store_.policy_->set_request_token("fake token");
  store_.policy_->set_device_id("fake client id");
  store_.policy_->set_timestamp(32);
  store_.policy_->set_valid_serial_number_missing(true);
  store_.policy_->set_public_key_version(17);
  EXPECT_CALL(client_,
              SetupRegistration(store_.policy_->request_token(),
                                store_.policy_->device_id())).Times(1);
  store_.NotifyStoreLoaded();
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromMilliseconds(32),
            client_.last_policy_timestamp_);
  EXPECT_TRUE(client_.submit_machine_id_);
  EXPECT_TRUE(client_.public_key_version_valid_);
  EXPECT_EQ(17, client_.public_key_version_);
}

TEST_F(CloudPolicyServiceTest, PolicyUpdateClientFailure) {
  client_.SetStatus(DM_STATUS_REQUEST_FAILED);
  EXPECT_CALL(store_, Store(_)).Times(0);
  client_.NotifyPolicyFetched();
}

TEST_F(CloudPolicyServiceTest, RefreshPolicySuccess) {
  testing::InSequence seq;

  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  client_.SetDMToken("fake token");

  // Trigger a fetch on the client.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  service_.RefreshPolicy(base::Bind(&CloudPolicyServiceTest::OnPolicyRefresh,
                                    base::Unretained(this)));

  // Client responds, push policy to store.
  em::PolicyFetchResponse policy;
  policy.set_policy_data("fake policy");
  client_.SetPolicy(policy_ns_key_, policy);
  client_.fetched_invalidation_version_ = 12345;
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  EXPECT_EQ(0, store_.invalidation_version());
  client_.NotifyPolicyFetched();
  EXPECT_EQ(12345, store_.invalidation_version());

  // Store reloads policy, callback gets triggered.
  store_.policy_.reset(new em::PolicyData());
  store_.policy_->set_request_token("token");
  store_.policy_->set_device_id("device-id");
  EXPECT_CALL(*this, OnPolicyRefresh(true)).Times(1);
  store_.NotifyStoreLoaded();
}

TEST_F(CloudPolicyServiceTest, RefreshPolicyNotRegistered) {
  // Clear the token so the client is not registered.
  client_.SetDMToken(std::string());

  EXPECT_CALL(client_, FetchPolicy()).Times(0);
  EXPECT_CALL(*this, OnPolicyRefresh(false)).Times(1);
  service_.RefreshPolicy(base::Bind(&CloudPolicyServiceTest::OnPolicyRefresh,
                                    base::Unretained(this)));
}

TEST_F(CloudPolicyServiceTest, RefreshPolicyClientError) {
  testing::InSequence seq;

  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  client_.SetDMToken("fake token");

  // Trigger a fetch on the client.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  service_.RefreshPolicy(base::Bind(&CloudPolicyServiceTest::OnPolicyRefresh,
                                    base::Unretained(this)));

  // Client responds with an error, which should trigger the callback.
  client_.SetStatus(DM_STATUS_REQUEST_FAILED);
  EXPECT_CALL(*this, OnPolicyRefresh(false)).Times(1);
  client_.NotifyClientError();
}

TEST_F(CloudPolicyServiceTest, RefreshPolicyStoreError) {
  testing::InSequence seq;

  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  client_.SetDMToken("fake token");

  // Trigger a fetch on the client.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  service_.RefreshPolicy(base::Bind(&CloudPolicyServiceTest::OnPolicyRefresh,
                                    base::Unretained(this)));

  // Client responds, push policy to store.
  em::PolicyFetchResponse policy;
  policy.set_policy_data("fake policy");
  client_.SetPolicy(policy_ns_key_, policy);
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  client_.NotifyPolicyFetched();

  // Store fails, which should trigger the callback.
  EXPECT_CALL(*this, OnPolicyRefresh(false)).Times(1);
  store_.NotifyStoreError();
}

TEST_F(CloudPolicyServiceTest, RefreshPolicyConcurrent) {
  testing::InSequence seq;

  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  client_.SetDMToken("fake token");

  // Trigger a fetch on the client.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  service_.RefreshPolicy(base::Bind(&CloudPolicyServiceTest::OnPolicyRefresh,
                                    base::Unretained(this)));

  // Triggering another policy refresh should generate a new fetch request.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  service_.RefreshPolicy(base::Bind(&CloudPolicyServiceTest::OnPolicyRefresh,
                                    base::Unretained(this)));

  // Client responds, push policy to store.
  em::PolicyFetchResponse policy;
  policy.set_policy_data("fake policy");
  client_.SetPolicy(policy_ns_key_, policy);
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  client_.NotifyPolicyFetched();

  // Trigger another policy fetch.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  service_.RefreshPolicy(base::Bind(&CloudPolicyServiceTest::OnPolicyRefresh,
                                    base::Unretained(this)));

  // The store finishing the first load should not generate callbacks.
  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  store_.NotifyStoreLoaded();

  // Second policy fetch finishes.
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  client_.NotifyPolicyFetched();

  // Corresponding store operation finishes, all _three_ callbacks fire.
  EXPECT_CALL(*this, OnPolicyRefresh(true)).Times(3);
  store_.NotifyStoreLoaded();
}

TEST_F(CloudPolicyServiceTest, StoreAlreadyInitialized) {
  // Service should start off initialized if the store has already loaded
  // policy.
  store_.NotifyStoreLoaded();
  CloudPolicyService service(policy_ns_key_, &client_, &store_);
  EXPECT_TRUE(service.IsInitializationComplete());
}

TEST_F(CloudPolicyServiceTest, StoreLoadAfterCreation) {
  // Service should start off un-initialized if the store has not yet loaded
  // policy.
  EXPECT_FALSE(service_.IsInitializationComplete());
  MockCloudPolicyServiceObserver observer;
  service_.AddObserver(&observer);
  // Service should be marked as initialized and observer should be called back.
  EXPECT_CALL(observer, OnInitializationCompleted(&service_)).Times(1);
  store_.NotifyStoreLoaded();
  EXPECT_TRUE(service_.IsInitializationComplete());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Now, the next time the store is loaded, the observer should not be called
  // again.
  EXPECT_CALL(observer, OnInitializationCompleted(&service_)).Times(0);
  store_.NotifyStoreLoaded();
  service_.RemoveObserver(&observer);
}

}  // namespace policy
