// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_store.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/core/account_id/account_id.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::Mock;

namespace policy {

namespace {

const char kTestExtension[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestDownload[] = "http://example.com/getpolicy?id=123";
const char kTestPolicy[] =
    "{"
    "  \"Name\": {"
    "    \"Value\": \"disabled\""
    "  },"
    "  \"Second\": {"
    "    \"Value\": \"maybe\","
    "    \"Level\": \"Recommended\""
    "  }"
    "}";

std::string TestPolicyHash() {
  return crypto::SHA256HashString(kTestPolicy);
}

bool NotEqual(const std::string& expected, const std::string& key) {
  return key != expected;
}

bool True(const std::string& ignored) {
  return true;
}

class MockComponentCloudPolicyStoreDelegate
    : public ComponentCloudPolicyStore::Delegate {
 public:
  virtual ~MockComponentCloudPolicyStoreDelegate() {}

  MOCK_METHOD0(OnComponentCloudPolicyStoreUpdated, void());
};

}  // namespace

class ComponentCloudPolicyStoreTest : public testing::Test {
 protected:
  ComponentCloudPolicyStoreTest()
      : kTestPolicyNS(POLICY_DOMAIN_EXTENSIONS, kTestExtension) {
    builder_.SetDefaultSigningKey();
    builder_.policy_data().set_policy_type(
        dm_protocol::kChromeExtensionPolicyType);
    builder_.policy_data().set_settings_entity_id(kTestExtension);
    builder_.payload().set_download_url(kTestDownload);
    builder_.payload().set_secure_hash(TestPolicyHash());

    public_key_ = builder_.GetPublicSigningKeyAsString();

    PolicyMap& policy = expected_bundle_.Get(kTestPolicyNS);
    policy.Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("disabled"),
               nullptr);
    policy.Set("Second", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("maybe"),
               nullptr);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_.reset(
        new ResourceCache(temp_dir_.GetPath(),
                          base::MakeRefCounted<base::TestSimpleTaskRunner>()));
    store_.reset(new ComponentCloudPolicyStore(&store_delegate_, cache_.get()));
    store_->SetCredentials(ComponentPolicyBuilder::GetFakeAccountId(),
                           ComponentPolicyBuilder::kFakeToken,
                           ComponentPolicyBuilder::kFakeDeviceId, public_key_,
                           ComponentPolicyBuilder::kFakePublicKeyVersion);
  }

  std::unique_ptr<em::PolicyFetchResponse> CreateResponse() {
    builder_.Build();
    return std::make_unique<em::PolicyFetchResponse>(builder_.policy());
  }

  std::unique_ptr<em::PolicyData> CreatePolicyData() {
    builder_.Build();
    return std::make_unique<em::PolicyData>(builder_.policy_data());
  }

  std::string CreateSerializedResponse() {
    builder_.Build();
    return builder_.GetBlob();
  }

  // Returns true if the policy exposed by the |store| is empty.
  bool IsStoreEmpty(const ComponentCloudPolicyStore& store) {
    return store.policy().Equals(PolicyBundle());
  }

  void StoreTestPolicy(ComponentCloudPolicyStore* store) {
    EXPECT_TRUE(store->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
    EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
    EXPECT_TRUE(store->Store(kTestPolicyNS, CreateSerializedResponse(),
                             CreatePolicyData().get(), TestPolicyHash(),
                             kTestPolicy));
    Mock::VerifyAndClearExpectations(&store_delegate_);
    EXPECT_TRUE(store->policy().Equals(expected_bundle_));
    EXPECT_FALSE(LoadCacheExtensionsSubkeys().empty());
  }

  std::map<std::string, std::string> LoadCacheExtensionsSubkeys() {
    std::map<std::string, std::string> contents;
    cache_->LoadAllSubkeys("extension-policy", &contents);
    return contents;
  }

  const PolicyNamespace kTestPolicyNS;
  std::unique_ptr<ResourceCache> cache_;
  std::unique_ptr<ComponentCloudPolicyStore> store_;
  MockComponentCloudPolicyStoreDelegate store_delegate_;
  ComponentPolicyBuilder builder_;
  PolicyBundle expected_bundle_;
  std::string public_key_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicy) {
  em::PolicyData policy_data;
  em::ExternalPolicyData payload;
  EXPECT_TRUE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                     &policy_data, &payload));
  EXPECT_EQ(dm_protocol::kChromeExtensionPolicyType, policy_data.policy_type());
  EXPECT_EQ(kTestExtension, policy_data.settings_entity_id());
  EXPECT_EQ(kTestDownload, payload.download_url());
  EXPECT_EQ(TestPolicyHash(), payload.secure_hash());
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongTimestamp) {
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                            CreatePolicyData().get(), TestPolicyHash(),
                            kTestPolicy));

  const int64_t kPastTimestamp =
      (base::Time() + base::TimeDelta::FromDays(1)).ToJavaTime();
  CHECK_GT(ComponentPolicyBuilder::kFakeTimestamp, kPastTimestamp);
  builder_.policy_data().set_timestamp(kPastTimestamp);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongUser) {
  builder_.policy_data().set_username("anotheruser@example.com");
  builder_.policy_data().set_gaia_id("another-gaia-id");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongDMToken) {
  builder_.policy_data().set_request_token("notmytoken");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongDeviceId) {
  builder_.policy_data().set_device_id("invalid");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadType) {
  builder_.policy_data().set_policy_type(dm_protocol::kChromeUserPolicyType);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongNamespace) {
  EXPECT_FALSE(store_->ValidatePolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "nosuchid"), CreateResponse(),
      nullptr /* policy_data */, nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyNoSignature) {
  builder_.UnsetSigningKey();
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadSignature) {
  std::unique_ptr<em::PolicyFetchResponse> response = CreateResponse();
  response->set_policy_data_signature("invalid");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, std::move(response),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyEmptyComponentId) {
  builder_.policy_data().set_settings_entity_id(std::string());
  EXPECT_FALSE(store_->ValidatePolicy(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, std::string()),
      CreateResponse(), nullptr /* policy_data */, nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongPublicKey) {
  // Test against a policy signed with a wrong key.
  builder_.SetSigningKey(*ComponentPolicyBuilder::CreateTestOtherSigningKey());
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongPublicKeyVersion) {
  // Test against a policy containing wrong public key version.
  builder_.policy_data().set_public_key_version(
      ComponentPolicyBuilder::kFakePublicKeyVersion + 1);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyDifferentPublicKey) {
  // Test against a policy signed with a different key and containing a new
  // public key version.
  builder_.SetSigningKey(*ComponentPolicyBuilder::CreateTestOtherSigningKey());
  builder_.policy_data().set_public_key_version(
      ComponentPolicyBuilder::kFakePublicKeyVersion + 1);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadDownloadUrl) {
  builder_.payload().set_download_url("invalidurl");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyEmptyDownloadUrl) {
  builder_.payload().clear_download_url();
  builder_.payload().clear_secure_hash();
  // This is valid; it's how "no policy" is signalled to the client.
  EXPECT_TRUE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                     nullptr /* policy_data */,
                                     nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadPayload) {
  builder_.clear_payload();
  builder_.policy_data().set_policy_value("broken");
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentials) {
  store_.reset(new ComponentCloudPolicyStore(&store_delegate_, cache_.get()));
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsUser) {
  store_.reset(new ComponentCloudPolicyStore(&store_delegate_, cache_.get()));
  store_->SetCredentials(AccountId(), ComponentPolicyBuilder::kFakeToken,
                         ComponentPolicyBuilder::kFakeDeviceId, public_key_,
                         ComponentPolicyBuilder::kFakePublicKeyVersion);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsDMToken) {
  store_.reset(new ComponentCloudPolicyStore(&store_delegate_, cache_.get()));
  store_->SetCredentials(ComponentPolicyBuilder::GetFakeAccountId(),
                         std::string() /* dm_token */,
                         ComponentPolicyBuilder::kFakeDeviceId, public_key_,
                         ComponentPolicyBuilder::kFakePublicKeyVersion);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsDeviceId) {
  store_.reset(new ComponentCloudPolicyStore(&store_delegate_, cache_.get()));
  store_->SetCredentials(ComponentPolicyBuilder::GetFakeAccountId(),
                         ComponentPolicyBuilder::kFakeToken,
                         std::string() /* device_id */, public_key_,
                         ComponentPolicyBuilder::kFakePublicKeyVersion);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsPublicKey) {
  store_.reset(new ComponentCloudPolicyStore(&store_delegate_, cache_.get()));
  store_->SetCredentials(ComponentPolicyBuilder::GetFakeAccountId(),
                         ComponentPolicyBuilder::kFakeToken,
                         ComponentPolicyBuilder::kFakeDeviceId,
                         std::string() /* public_key */,
                         ComponentPolicyBuilder::kFakePublicKeyVersion);
  EXPECT_FALSE(store_->ValidatePolicy(kTestPolicyNS, CreateResponse(),
                                      nullptr /* policy_data */,
                                      nullptr /* payload */));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentialsPublicKeyVersion) {
  StoreTestPolicy(store_.get());
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  another_store.SetCredentials(ComponentPolicyBuilder::GetFakeAccountId(),
                               ComponentPolicyBuilder::kFakeToken,
                               ComponentPolicyBuilder::kFakeDeviceId,
                               public_key_, -1 /* public_key_version */);
  another_store.Load();
  EXPECT_TRUE(IsStoreEmpty(another_store));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateWrongCredentialsDMToken) {
  StoreTestPolicy(store_.get());
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  another_store.SetCredentials(
      ComponentPolicyBuilder::GetFakeAccountId(), "wrongtoken",
      ComponentPolicyBuilder::kFakeDeviceId, public_key_,
      ComponentPolicyBuilder::kFakePublicKeyVersion);
  another_store.Load();
  EXPECT_TRUE(IsStoreEmpty(another_store));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateWrongCredentialsDeviceId) {
  StoreTestPolicy(store_.get());
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  another_store.SetCredentials(ComponentPolicyBuilder::GetFakeAccountId(),
                               ComponentPolicyBuilder::kFakeToken,
                               "wrongdeviceid", public_key_,
                               ComponentPolicyBuilder::kFakePublicKeyVersion);
  another_store.Load();
  EXPECT_TRUE(IsStoreEmpty(another_store));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateWrongCredentialsPublicKey) {
  StoreTestPolicy(store_.get());
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  another_store.SetCredentials(
      ComponentPolicyBuilder::GetFakeAccountId(),
      ComponentPolicyBuilder::kFakeToken, ComponentPolicyBuilder::kFakeDeviceId,
      "wrongkey", ComponentPolicyBuilder::kFakePublicKeyVersion);
  another_store.Load();
  EXPECT_TRUE(IsStoreEmpty(another_store));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest,
       ValidateWrongCredentialsPublicKeyVersion) {
  StoreTestPolicy(store_.get());
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  another_store.SetCredentials(
      ComponentPolicyBuilder::GetFakeAccountId(),
      ComponentPolicyBuilder::kFakeToken, ComponentPolicyBuilder::kFakeDeviceId,
      public_key_, ComponentPolicyBuilder::kFakePublicKeyVersion + 1);
  another_store.Load();
  EXPECT_TRUE(IsStoreEmpty(another_store));
  EXPECT_TRUE(LoadCacheExtensionsSubkeys().empty());
}

TEST_F(ComponentCloudPolicyStoreTest, StoreAndLoad) {
  // Initially empty.
  EXPECT_TRUE(IsStoreEmpty(*store_));
  store_->Load();
  EXPECT_TRUE(IsStoreEmpty(*store_));

  // Store policy for an unsupported domain.
  builder_.policy_data().set_policy_type(dm_protocol::kChromeUserPolicyType);
  EXPECT_FALSE(
      store_->Store(PolicyNamespace(POLICY_DOMAIN_CHROME, kTestExtension),
                    CreateSerializedResponse(), CreatePolicyData().get(),
                    TestPolicyHash(), kTestPolicy));

  // Store policy with the wrong hash.
  builder_.policy_data().set_policy_type(
      dm_protocol::kChromeExtensionPolicyType);
  builder_.payload().set_secure_hash("badash");
  EXPECT_FALSE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                             CreatePolicyData().get(), "badash", kTestPolicy));

  // Store policy without a hash.
  builder_.payload().clear_secure_hash();
  EXPECT_FALSE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                             CreatePolicyData().get(), std::string(),
                             kTestPolicy));

  // Store policy with invalid JSON data.
  static const char kInvalidData[] = "{ not json }";
  const std::string invalid_data_hash = crypto::SHA256HashString(kInvalidData);
  builder_.payload().set_secure_hash(invalid_data_hash);
  EXPECT_FALSE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                             CreatePolicyData().get(), invalid_data_hash,
                             kInvalidData));

  // All of those failed.
  EXPECT_TRUE(IsStoreEmpty(*store_));
  EXPECT_EQ(std::string(), store_->GetCachedHash(kTestPolicyNS));

  // Now store a valid policy.
  builder_.payload().set_secure_hash(TestPolicyHash());
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                            CreatePolicyData().get(), TestPolicyHash(),
                            kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(IsStoreEmpty(*store_));
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));
  EXPECT_EQ(TestPolicyHash(), store_->GetCachedHash(kTestPolicyNS));

  // Loading from the cache validates the policy data again.
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  another_store.SetCredentials(
      ComponentPolicyBuilder::GetFakeAccountId(),
      ComponentPolicyBuilder::kFakeToken, ComponentPolicyBuilder::kFakeDeviceId,
      public_key_, ComponentPolicyBuilder::kFakePublicKeyVersion);
  another_store.Load();
  EXPECT_TRUE(another_store.policy().Equals(expected_bundle_));
  EXPECT_EQ(TestPolicyHash(), another_store.GetCachedHash(kTestPolicyNS));
}

TEST_F(ComponentCloudPolicyStoreTest, Updates) {
  // Store some policies.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                            CreatePolicyData().get(), TestPolicyHash(),
                            kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(IsStoreEmpty(*store_));
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Deleting a non-existant namespace doesn't trigger updates.
  PolicyNamespace ns_fake(POLICY_DOMAIN_EXTENSIONS, "nosuchid");
  store_->Delete(ns_fake);
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Deleting a namespace that has policies triggers an update.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  store_->Delete(kTestPolicyNS);
  Mock::VerifyAndClearExpectations(&store_delegate_);
}

TEST_F(ComponentCloudPolicyStoreTest, Purge) {
  // Store a valid policy.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(kTestPolicyNS, CreateSerializedResponse(),
                            CreatePolicyData().get(), TestPolicyHash(),
                            kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(IsStoreEmpty(*store_));
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Purge other components.
  store_->Purge(POLICY_DOMAIN_EXTENSIONS,
                base::Bind(&NotEqual, kTestExtension));

  // The policy for |kTestPolicyNS| is still served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Loading the store again will still see |kTestPolicyNS|.
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(another_store.policy().Equals(empty_bundle));
  another_store.SetCredentials(
      ComponentPolicyBuilder::GetFakeAccountId(),
      ComponentPolicyBuilder::kFakeToken, ComponentPolicyBuilder::kFakeDeviceId,
      public_key_, ComponentPolicyBuilder::kFakePublicKeyVersion);
  another_store.Load();
  EXPECT_TRUE(another_store.policy().Equals(expected_bundle_));

  // Now purge everything.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  store_->Purge(POLICY_DOMAIN_EXTENSIONS, base::Bind(&True));
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // No policies are served anymore.
  EXPECT_TRUE(store_->policy().Equals(empty_bundle));

  // And they aren't loaded anymore either.
  ComponentCloudPolicyStore yet_another_store(&store_delegate_, cache_.get());
  yet_another_store.SetCredentials(
      ComponentPolicyBuilder::GetFakeAccountId(),
      ComponentPolicyBuilder::kFakeToken, ComponentPolicyBuilder::kFakeDeviceId,
      public_key_, ComponentPolicyBuilder::kFakePublicKeyVersion);
  yet_another_store.Load();
  EXPECT_TRUE(yet_another_store.policy().Equals(empty_bundle));
}

}  // namespace policy
