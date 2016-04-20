// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_store.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_types.h"
#include "crypto/sha2.h"
#include "policy/proto/chrome_extension_policy.pb.h"
#include "policy/proto/device_management_backend.pb.h"
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
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_.reset(new ResourceCache(
        temp_dir_.path(),
        make_scoped_refptr(new base::TestSimpleTaskRunner)));
    store_.reset(new ComponentCloudPolicyStore(&store_delegate_, cache_.get()));
    store_->SetCredentials(ComponentPolicyBuilder::kFakeUsername,
                           ComponentPolicyBuilder::kFakeToken);

    builder_.policy_data().set_policy_type(
        dm_protocol::kChromeExtensionPolicyType);
    builder_.policy_data().set_settings_entity_id(kTestExtension);
    builder_.payload().set_download_url(kTestDownload);
    builder_.payload().set_secure_hash(TestPolicyHash());

    PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
    PolicyMap& policy = expected_bundle_.Get(ns);
    policy.Set("Name",
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD,
               new base::StringValue("disabled"),
               NULL);
    policy.Set("Second",
               POLICY_LEVEL_RECOMMENDED,
               POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD,
               new base::StringValue("maybe"),
               NULL);
  }

  // Returns true if the policy exposed by the |store_| is empty.
  bool IsEmpty() {
    return store_->policy().begin() == store_->policy().end();
  }

  std::unique_ptr<em::PolicyFetchResponse> CreateResponse() {
    builder_.Build();
    return base::WrapUnique(new em::PolicyFetchResponse(builder_.policy()));
  }

  std::string CreateSerializedResponse() {
    builder_.Build();
    return builder_.GetBlob();
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ResourceCache> cache_;
  std::unique_ptr<ComponentCloudPolicyStore> store_;
  MockComponentCloudPolicyStoreDelegate store_delegate_;
  ComponentPolicyBuilder builder_;
  PolicyBundle expected_bundle_;
};

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicy) {
  em::ExternalPolicyData payload;
  PolicyNamespace ns;
  EXPECT_TRUE(store_->ValidatePolicy(CreateResponse(), &ns, &payload));
  EXPECT_EQ(POLICY_DOMAIN_EXTENSIONS, ns.domain);
  EXPECT_EQ(kTestExtension, ns.component_id);
  EXPECT_EQ(kTestDownload, payload.download_url());
  EXPECT_EQ(TestPolicyHash(), payload.secure_hash());
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongUsername) {
  builder_.policy_data().set_username("anotheruser@example.com");
  em::ExternalPolicyData payload;
  PolicyNamespace ns;
  EXPECT_FALSE(store_->ValidatePolicy(CreateResponse(), &ns, &payload));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyWrongDMToken) {
  builder_.policy_data().set_request_token("notmytoken");
  em::ExternalPolicyData payload;
  PolicyNamespace ns;
  EXPECT_FALSE(store_->ValidatePolicy(CreateResponse(), &ns, &payload));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadType) {
  builder_.policy_data().set_policy_type(dm_protocol::kChromeUserPolicyType);
  em::ExternalPolicyData payload;
  PolicyNamespace ns;
  EXPECT_FALSE(store_->ValidatePolicy(CreateResponse(), &ns, &payload));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadDownloadUrl) {
  builder_.payload().set_download_url("invalidurl");
  em::ExternalPolicyData payload;
  PolicyNamespace ns;
  EXPECT_FALSE(store_->ValidatePolicy(CreateResponse(), &ns, &payload));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyEmptyDownloadUrl) {
  builder_.payload().clear_download_url();
  builder_.payload().clear_secure_hash();
  em::ExternalPolicyData payload;
  PolicyNamespace ns;
  // This is valid; it's how "no policy" is signalled to the client.
  EXPECT_TRUE(store_->ValidatePolicy(CreateResponse(), &ns, &payload));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidatePolicyBadPayload) {
  builder_.clear_payload();
  builder_.policy_data().set_policy_value("broken");
  em::ExternalPolicyData payload;
  PolicyNamespace ns;
  EXPECT_FALSE(store_->ValidatePolicy(CreateResponse(), &ns, &payload));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateNoCredentials) {
  store_.reset(new ComponentCloudPolicyStore(&store_delegate_, cache_.get()));
  em::ExternalPolicyData payload;
  PolicyNamespace ns;
  EXPECT_FALSE(store_->ValidatePolicy(CreateResponse(), &ns, &payload));
}

TEST_F(ComponentCloudPolicyStoreTest, ValidateWrongCredentials) {
  em::ExternalPolicyData payload;
  PolicyNamespace ns;
  // Verify that the default response validates with the right credentials.
  EXPECT_TRUE(store_->ValidatePolicy(CreateResponse(), &ns, &payload));
  // Now store that response.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(
      ns, CreateSerializedResponse(), TestPolicyHash(), kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));
  // And verify that the response data in the cache.
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys("extension-policy", &contents);
  EXPECT_FALSE(contents.empty());

  // Try loading the cached response data with wrong credentials.
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  another_store.SetCredentials("wrongdude@example.com", "wrongtoken");
  another_store.Load();
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(another_store.policy().Equals(empty_bundle));

  // The failure to read wiped the cache.
  cache_->LoadAllSubkeys("extension-policy", &contents);
  EXPECT_TRUE(contents.empty());
}

TEST_F(ComponentCloudPolicyStoreTest, StoreAndLoad) {
  // Initially empty.
  EXPECT_TRUE(IsEmpty());
  store_->Load();
  EXPECT_TRUE(IsEmpty());

  // Store policy for an unsupported domain.
  PolicyNamespace ns(POLICY_DOMAIN_CHROME, kTestExtension);
  builder_.policy_data().set_policy_type(dm_protocol::kChromeUserPolicyType);
  EXPECT_FALSE(store_->Store(
      ns, CreateSerializedResponse(), TestPolicyHash(), kTestPolicy));

  // Store policy with the wrong hash.
  builder_.policy_data().set_policy_type(
      dm_protocol::kChromeExtensionPolicyType);
  ns.domain = POLICY_DOMAIN_EXTENSIONS;
  builder_.payload().set_secure_hash("badash");
  EXPECT_FALSE(store_->Store(
      ns, CreateSerializedResponse(), "badash", kTestPolicy));

  // Store policy without a hash.
  builder_.payload().clear_secure_hash();
  EXPECT_FALSE(store_->Store(
      ns, CreateSerializedResponse(), std::string(), kTestPolicy));

  // Store policy with invalid JSON data.
  static const char kInvalidData[] = "{ not json }";
  const std::string invalid_data_hash = crypto::SHA256HashString(kInvalidData);
  builder_.payload().set_secure_hash(invalid_data_hash);
  EXPECT_FALSE(store_->Store(
      ns, CreateSerializedResponse(), invalid_data_hash, kInvalidData));

  // All of those failed.
  EXPECT_TRUE(IsEmpty());
  EXPECT_EQ(std::string(), store_->GetCachedHash(ns));

  // Now store a valid policy.
  builder_.payload().set_secure_hash(TestPolicyHash());
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(
      ns, CreateSerializedResponse(), TestPolicyHash(), kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(IsEmpty());
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));
  EXPECT_EQ(TestPolicyHash(), store_->GetCachedHash(ns));

  // Loading from the cache validates the policy data again.
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  another_store.SetCredentials(ComponentPolicyBuilder::kFakeUsername,
                               ComponentPolicyBuilder::kFakeToken);
  another_store.Load();
  EXPECT_TRUE(another_store.policy().Equals(expected_bundle_));
  EXPECT_EQ(TestPolicyHash(), another_store.GetCachedHash(ns));
}

TEST_F(ComponentCloudPolicyStoreTest, Updates) {
  // Store some policies.
  PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(
      ns, CreateSerializedResponse(), TestPolicyHash(), kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(IsEmpty());
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Deleting a non-existant namespace doesn't trigger updates.
  PolicyNamespace ns_fake(POLICY_DOMAIN_EXTENSIONS, "nosuchid");
  store_->Delete(ns_fake);
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Deleting a namespace that has policies triggers an update.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  store_->Delete(ns);
  Mock::VerifyAndClearExpectations(&store_delegate_);
}

TEST_F(ComponentCloudPolicyStoreTest, Purge) {
  // Store a valid policy.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  EXPECT_TRUE(store_->Store(
      ns, CreateSerializedResponse(), TestPolicyHash(), kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(IsEmpty());
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Purge other components.
  store_->Purge(POLICY_DOMAIN_EXTENSIONS,
                base::Bind(&NotEqual, kTestExtension));

  // The policy for |ns| is still served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Loading the store again will still see |ns|.
  ComponentCloudPolicyStore another_store(&store_delegate_, cache_.get());
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(another_store.policy().Equals(empty_bundle));
  another_store.SetCredentials(ComponentPolicyBuilder::kFakeUsername,
                               ComponentPolicyBuilder::kFakeToken);
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
  yet_another_store.SetCredentials(ComponentPolicyBuilder::kFakeUsername,
                                   ComponentPolicyBuilder::kFakeToken);
  yet_another_store.Load();
  EXPECT_TRUE(yet_another_store.policy().Equals(empty_bundle));
}

}  // namespace policy
