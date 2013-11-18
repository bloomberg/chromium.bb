// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/component_cloud_policy_service.h"

#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sha1.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_client.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_store.h"
#include "chrome/browser/policy/cloud/policy_builder.h"
#include "chrome/browser/policy/cloud/resource_cache.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/policy/proto/cloud/chrome_extension_policy.pb.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "chrome/browser/policy/schema_map.h"
#include "components/policy/core/common/schema.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::Mock;

namespace policy {

namespace {

const char kTestExtension[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtension2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
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

const char kTestSchema[] =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"Name\": { \"type\": \"string\" },"
    "    \"Second\": { \"type\": \"string\" }"
    "  }"
    "}";

class MockComponentCloudPolicyDelegate
    : public ComponentCloudPolicyService::Delegate {
 public:
  virtual ~MockComponentCloudPolicyDelegate() {}

  MOCK_METHOD0(OnComponentCloudPolicyRefreshNeeded, void());
  MOCK_METHOD0(OnComponentCloudPolicyUpdated, void());
};

class TestURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit TestURLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {}
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    return NULL;
  }
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE {
    return task_runner_;
  }

 private:
  virtual ~TestURLRequestContextGetter() {}

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace

class ComponentCloudPolicyServiceTest : public testing::Test {
 protected:
  ComponentCloudPolicyServiceTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    cache_ = new ResourceCache(temp_dir_.path(), loop_.message_loop_proxy());
    request_context_ =
        new TestURLRequestContextGetter(loop_.message_loop_proxy());
    service_.reset(new ComponentCloudPolicyService(
        &delegate_,
        &registry_,
        &store_,
        make_scoped_ptr(cache_),
        &client_,
        request_context_,
        loop_.message_loop_proxy(),
        loop_.message_loop_proxy()));

    builder_.policy_data().set_policy_type(
        dm_protocol::kChromeExtensionPolicyType);
    builder_.policy_data().set_settings_entity_id(kTestExtension);
    builder_.payload().set_download_url(kTestDownload);
    builder_.payload().set_secure_hash(base::SHA1HashString(kTestPolicy));

    expected_policy_.Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         base::Value::CreateStringValue("disabled"), NULL);
    expected_policy_.Set("Second", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                         base::Value::CreateStringValue("maybe"), NULL);

    EXPECT_EQ(0u, client_.namespaces_to_fetch_.size());
  }

  virtual void TearDown() OVERRIDE {
    // The service cleans up its backend on the background thread.
    service_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
  }

  void LoadStore() {
    EXPECT_FALSE(store_.is_initialized());

    em::PolicyData* data = new em::PolicyData();
    data->set_username(ComponentPolicyBuilder::kFakeUsername);
    data->set_request_token(ComponentPolicyBuilder::kFakeToken);
    store_.policy_.reset(data);

    store_.NotifyStoreLoaded();
    RunUntilIdle();
    EXPECT_TRUE(store_.is_initialized());
  }

  void InitializeRegistry() {
    registry_.RegisterComponent(
        PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension),
        CreateTestSchema());
    registry_.SetReady(POLICY_DOMAIN_CHROME);
    registry_.SetReady(POLICY_DOMAIN_EXTENSIONS);
  }

  void PopulateCache() {
    EXPECT_TRUE(cache_->Store(
        "extension-policy", kTestExtension, CreateSerializedResponse()));
    EXPECT_TRUE(
        cache_->Store("extension-policy-data", kTestExtension, kTestPolicy));

    builder_.policy_data().set_settings_entity_id(kTestExtension2);
    EXPECT_TRUE(cache_->Store(
        "extension-policy", kTestExtension2, CreateSerializedResponse()));
    EXPECT_TRUE(
        cache_->Store("extension-policy-data", kTestExtension2, kTestPolicy));
  }

  scoped_ptr<em::PolicyFetchResponse> CreateResponse() {
    builder_.Build();
    return make_scoped_ptr(new em::PolicyFetchResponse(builder_.policy()));
  }

  std::string CreateSerializedResponse() {
    builder_.Build();
    return builder_.GetBlob();
  }

  Schema CreateTestSchema() {
    std::string error;
    Schema schema = Schema::Parse(kTestSchema, &error);
    EXPECT_TRUE(schema.valid()) << error;
    return schema;
  }

  base::MessageLoop loop_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory fetcher_factory_;
  MockComponentCloudPolicyDelegate delegate_;
  // |cache_| is owned by the |service_| and is invalid once the |service_|
  // is destroyed.
  ResourceCache* cache_;
  MockCloudPolicyClient client_;
  MockCloudPolicyStore store_;
  SchemaRegistry registry_;
  scoped_ptr<ComponentCloudPolicyService> service_;
  ComponentPolicyBuilder builder_;
  PolicyMap expected_policy_;
};

TEST_F(ComponentCloudPolicyServiceTest, InitializedAtConstructionTime) {
  service_.reset();
  LoadStore();
  InitializeRegistry();

  cache_ = new ResourceCache(temp_dir_.path(), loop_.message_loop_proxy());
  service_.reset(new ComponentCloudPolicyService(&delegate_,
                                                 &registry_,
                                                 &store_,
                                                 make_scoped_ptr(cache_),
                                                 &client_,
                                                 request_context_,
                                                 loop_.message_loop_proxy(),
                                                 loop_.message_loop_proxy()));
  EXPECT_FALSE(service_->is_initialized());

  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_TRUE(service_->is_initialized());
  EXPECT_EQ(1u, client_.namespaces_to_fetch_.size());
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(service_->policy().Equals(empty_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, InitializeStoreThenRegistry) {
  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded()).Times(0);
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated()).Times(0);
  LoadStore();
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_FALSE(service_->is_initialized());

  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  InitializeRegistry();
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());
  EXPECT_EQ(1u, client_.namespaces_to_fetch_.size());
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(service_->policy().Equals(empty_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, InitializeRegistryThenStore) {
  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded()).Times(0);
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated()).Times(0);
  InitializeRegistry();
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_FALSE(service_->is_initialized());

  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  LoadStore();
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());
  EXPECT_EQ(1u, client_.namespaces_to_fetch_.size());
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(service_->policy().Equals(empty_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, InitializeWithCachedPolicy) {
  PopulateCache();

  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  InitializeRegistry();
  LoadStore();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_TRUE(service_->is_initialized());
  EXPECT_EQ(1u, client_.namespaces_to_fetch_.size());

  // kTestExtension2 is not in the registry so it was dropped.
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys("extension-policy", &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(kTestExtension, contents.begin()->first);

  PolicyBundle expected_bundle;
  const PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  expected_bundle.Get(ns).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, FetchPolicy) {
  // Initialize the store and create the backend.
  // A refresh is not needed, because no components are registered yet.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded()).Times(0);
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  registry_.SetReady(POLICY_DOMAIN_CHROME);
  registry_.SetReady(POLICY_DOMAIN_EXTENSIONS);
  LoadStore();
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());

  // Register the components to fetch.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension),
      CreateTestSchema());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // Send back a fake policy fetch response.
  client_.SetPolicy(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType,
                                       kTestExtension),
                    *CreateResponse());
  service_->OnPolicyFetched(&client_);
  RunUntilIdle();

  // That should have triggered the download fetch.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());
  fetcher->set_response_code(200);
  fetcher->SetResponseString(kTestPolicy);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // The policy is now being served.
  const PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  PolicyBundle expected_bundle;
  expected_bundle.Get(ns).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, LoadAndPurgeCache) {
  // Insert data in the cache.
  PopulateCache();
  registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension2),
      CreateTestSchema());
  InitializeRegistry();

  // Load the initial cache.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  LoadStore();
  Mock::VerifyAndClearExpectations(&delegate_);

  PolicyBundle expected_bundle;
  PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  expected_bundle.Get(ns).CopyFrom(expected_policy_);
  ns.component_id = kTestExtension2;
  expected_bundle.Get(ns).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));

  // Now purge one of the extensions.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  registry_.UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension));
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  ns.component_id = kTestExtension;
  expected_bundle.Get(ns).Clear();
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));

  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys("extension-policy", &contents);
  EXPECT_EQ(1u, contents.size());
  EXPECT_TRUE(ContainsKey(contents, kTestExtension2));
}

TEST_F(ComponentCloudPolicyServiceTest, SignInAfterStartup) {
  registry_.SetReady(POLICY_DOMAIN_CHROME);
  registry_.SetReady(POLICY_DOMAIN_EXTENSIONS);

  // Do the same as LoadStore() but without the initial credentials.
  EXPECT_FALSE(store_.is_initialized());
  EXPECT_FALSE(service_->is_initialized());
  store_.policy_.reset(new em::PolicyData());  //  No credentials.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  store_.NotifyStoreLoaded();
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());

  // Register an extension.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension),
      CreateTestSchema());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // Send the response to the service. The response data will be ignored,
  // because the store doesn't have the updated credentials yet.
  client_.SetPolicy(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType,
                                       kTestExtension),
                    *CreateResponse());
  service_->OnPolicyFetched(&client_);
  RunUntilIdle();

  // The policy was ignored, and no download is started.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  EXPECT_FALSE(fetcher);

  // Now update the |store_| with the updated policy, which includes
  // credentials. The responses in the |client_| will be reloaded.
  em::PolicyData* data = new em::PolicyData();
  data->set_username(ComponentPolicyBuilder::kFakeUsername);
  data->set_request_token(ComponentPolicyBuilder::kFakeToken);
  store_.policy_.reset(data);
  store_.NotifyStoreLoaded();
  RunUntilIdle();

  // The extension policy was validated this time, and the download is started.
  fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());
  fetcher->set_response_code(200);
  fetcher->SetResponseString(kTestPolicy);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // The policy is now being served.
  PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  PolicyBundle expected_bundle;
  expected_bundle.Get(ns).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

}  // namespace policy
