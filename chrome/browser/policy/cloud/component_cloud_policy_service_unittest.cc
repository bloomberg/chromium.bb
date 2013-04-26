// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/component_cloud_policy_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/pickle.h"
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
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/policy/proto/cloud/chrome_extension_policy.pb.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
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
const char kTestExtension3[] = "cccccccccccccccccccccccccccccccc";
const char kTestDownload[] = "http://example.com/getpolicy?id=123";
const char kTestDownload2[] = "http://example.com/getpolicy?id=456";
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
  ComponentCloudPolicyServiceTest()
      : ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_ = new ResourceCache(temp_dir_.path());
    ASSERT_TRUE(cache_->IsOpen());
    service_.reset(new ComponentCloudPolicyService(
        &delegate_, &store_, make_scoped_ptr(cache_)));

    builder_.policy_data().set_policy_type(
        dm_protocol::kChromeExtensionPolicyType);
    builder_.policy_data().set_settings_entity_id(kTestExtension);
    builder_.payload().set_download_url(kTestDownload);
    builder_.payload().set_secure_hash(base::SHA1HashString(kTestPolicy));

    expected_policy_.Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         base::Value::CreateStringValue("disabled"));
    expected_policy_.Set("Second", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                         base::Value::CreateStringValue("maybe"));

    // A NULL |request_context_| is enough to construct the updater, but
    // ComponentCloudPolicyService::Backend::LoadStore() tests the pointer when
    // Connect() is called before the store was loaded.
    request_context_ =
        new TestURLRequestContextGetter(loop_.message_loop_proxy());
  }

  virtual void TearDown() OVERRIDE {
    // The service cleans up its backend on the FILE thread.
    service_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
  }

  void LoadStore() {
    EXPECT_FALSE(store_.is_initialized());
    EXPECT_FALSE(service_->is_initialized());

    em::PolicyData* data = new em::PolicyData();
    data->set_username(ComponentPolicyBuilder::kFakeUsername);
    data->set_request_token(ComponentPolicyBuilder::kFakeToken);
    store_.policy_.reset(data);

    EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
    store_.NotifyStoreLoaded();
    RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate_);
    EXPECT_TRUE(service_->is_initialized());
  }

  void PopulateCache() {
    Pickle pickle;
    pickle.WriteString(kTestExtension);
    pickle.WriteString(kTestExtension2);
    std::string data(reinterpret_cast<const char*>(pickle.data()),
                     pickle.size());
    EXPECT_TRUE(
        cache_->Store(ComponentCloudPolicyService::kComponentNamespaceCache,
                      dm_protocol::kChromeExtensionPolicyType,
                      data));

    EXPECT_TRUE(cache_->Store(
        "extension-policy", kTestExtension, CreateSerializedResponse()));
    EXPECT_TRUE(
        cache_->Store("extension-policy-data", kTestExtension, kTestPolicy));

    builder_.policy_data().set_settings_entity_id(kTestExtension2);
    EXPECT_TRUE(cache_->Store(
        "extension-policy", kTestExtension2, CreateSerializedResponse()));
    EXPECT_TRUE(
        cache_->Store("extension-policy-data", kTestExtension2, kTestPolicy));

    EXPECT_TRUE(cache_->Store("unrelated", "stuff", "here"));
  }

  scoped_ptr<em::PolicyFetchResponse> CreateResponse() {
    builder_.Build();
    return make_scoped_ptr(new em::PolicyFetchResponse(builder_.policy()));
  }

  std::string CreateSerializedResponse() {
    builder_.Build();
    return builder_.GetBlob();
  }

  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory fetcher_factory_;
  MockComponentCloudPolicyDelegate delegate_;
  // |cache_| is owned by the |service_| and is invalid once the |service_|
  // is destroyed.
  ResourceCache* cache_;
  MockCloudPolicyClient client_;
  MockCloudPolicyStore store_;
  scoped_ptr<ComponentCloudPolicyService> service_;
  ComponentPolicyBuilder builder_;
  PolicyMap expected_policy_;
};

TEST_F(ComponentCloudPolicyServiceTest, InitializeWithoutCredentials) {
  EXPECT_FALSE(service_->is_initialized());
  // Run the background task to initialize the backend.
  RunUntilIdle();
  // Still waiting for the |store_|.
  EXPECT_FALSE(service_->is_initialized());

  // Initialize with a store without credentials.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  store_.NotifyStoreLoaded();
  RunUntilIdle();
  EXPECT_TRUE(service_->is_initialized());
  Mock::VerifyAndClearExpectations(&delegate_);
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(service_->policy().Equals(empty_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, InitializationWithCachedComponents) {
  // Store a previous list of components.
  Pickle pickle;
  pickle.WriteString("aa");
  pickle.WriteString("bb");
  pickle.WriteString("cc");
  std::string data(reinterpret_cast<const char*>(pickle.data()), pickle.size());
  EXPECT_TRUE(
      cache_->Store(ComponentCloudPolicyService::kComponentNamespaceCache,
                    dm_protocol::kChromeExtensionPolicyType,
                    data));
  // Store some garbage in another domain; it won't be fetched.
  EXPECT_TRUE(cache_->Store(
      ComponentCloudPolicyService::kComponentNamespaceCache, "garbage", data));

  // Connect a client before initialization is complete.
  service_->Connect(&client_, request_context_);
  EXPECT_TRUE(client_.namespaces_to_fetch_.empty());

  // Now load the backend.
  LoadStore();

  // The cached namespaces were added to the client.
  std::set<PolicyNamespaceKey> set;
  set.insert(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType, "aa"));
  set.insert(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType, "bb"));
  set.insert(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType, "cc"));
  EXPECT_EQ(set, client_.namespaces_to_fetch_);

  // And the bad components were wiped from the cache.
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys(ComponentCloudPolicyService::kComponentNamespaceCache,
                         &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(std::string(dm_protocol::kChromeExtensionPolicyType),
            contents.begin()->first);
}

TEST_F(ComponentCloudPolicyServiceTest, ConnectAfterRegister) {
  // Add some components.
  std::set<std::string> components;
  components.insert(kTestExtension);
  components.insert(kTestExtension2);
  service_->RegisterPolicyDomain(POLICY_DOMAIN_EXTENSIONS, components);

  // Now connect the client.
  EXPECT_TRUE(client_.namespaces_to_fetch_.empty());
  service_->Connect(&client_, request_context_);
  EXPECT_TRUE(client_.namespaces_to_fetch_.empty());

  // It receives the namespaces once the backend is initialized.
  LoadStore();
  std::set<PolicyNamespaceKey> set;
  set.insert(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType,
                                kTestExtension));
  set.insert(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType,
                                kTestExtension2));
  EXPECT_EQ(set, client_.namespaces_to_fetch_);

  // The current components were also persisted to the cache.
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys(ComponentCloudPolicyService::kComponentNamespaceCache,
                         &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(std::string(dm_protocol::kChromeExtensionPolicyType),
            contents.begin()->first);
  const std::string data(contents.begin()->second);
  Pickle pickle(data.data(), data.size());
  PickleIterator pickit(pickle);
  std::string value0;
  std::string value1;
  std::string tmp;
  ASSERT_TRUE(pickit.ReadString(&value0));
  ASSERT_TRUE(pickit.ReadString(&value1));
  EXPECT_FALSE(pickit.ReadString(&tmp));
  std::set<std::string> unpickled;
  unpickled.insert(value0);
  unpickled.insert(value1);
  EXPECT_EQ(components, unpickled);
}

TEST_F(ComponentCloudPolicyServiceTest, StoreReadyAfterConnectAndRegister) {
  // Add some previous data to the cache.
  PopulateCache();

  // Add some components.
  std::set<std::string> components;
  components.insert(kTestExtension);
  service_->RegisterPolicyDomain(POLICY_DOMAIN_EXTENSIONS, components);

  // And connect the client. Make the client have some policies, with a new
  // download_url.
  builder_.payload().set_download_url(kTestDownload2);
  client_.SetPolicy(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType,
                                       kTestExtension),
                    *CreateResponse());
  service_->Connect(&client_, request_context_);

  // Now make the store ready.
  LoadStore();

  // The components that were registed before have their caches purged.
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys("extension-policy", &contents);
  EXPECT_EQ(1u, contents.size());  // kTestExtension2 was purged.
  EXPECT_TRUE(ContainsKey(contents, kTestExtension));
  cache_->LoadAllSubkeys("unrelated", &contents);
  EXPECT_EQ(1u, contents.size());  // unrelated keys are not purged.
  EXPECT_TRUE(ContainsKey(contents, "stuff"));

  // The policy responses that the client already had start updating.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  // Expect the URL of the client's PolicyFetchResponse, not the cached URL.
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());
}

TEST_F(ComponentCloudPolicyServiceTest, ConnectThenRegisterThenStoreReady) {
  // Connect right after creating the service.
  service_->Connect(&client_, request_context_);

  // Now register the current components, before the backend has been
  // initialized.
  EXPECT_TRUE(client_.namespaces_to_fetch_.empty());
  std::set<std::string> components;
  components.insert(kTestExtension);
  service_->RegisterPolicyDomain(POLICY_DOMAIN_EXTENSIONS, components);
  EXPECT_TRUE(client_.namespaces_to_fetch_.empty());

  // Now load the store. The client gets the namespaces.
  LoadStore();
  std::set<PolicyNamespaceKey> set;
  set.insert(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType,
                                kTestExtension));
  EXPECT_EQ(set, client_.namespaces_to_fetch_);
}

TEST_F(ComponentCloudPolicyServiceTest, FetchPolicy) {
  // Initialize the store and create the backend, and connect the client.
  LoadStore();
  // A refresh is not needed, because no components were found.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded()).Times(0);
  service_->Connect(&client_, request_context_);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Register the components to fetch.
  std::set<std::string> components;
  components.insert(kTestExtension);
  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  service_->RegisterPolicyDomain(POLICY_DOMAIN_EXTENSIONS, components);
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
  PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  PolicyBundle expected_bundle;
  expected_bundle.Get(ns).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));
}

TEST_F(ComponentCloudPolicyServiceTest, LoadAndPurgeCache) {
  // Insert data in the cache.
  PopulateCache();

  // Load the initial cache.
  LoadStore();

  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  service_->Connect(&client_, request_context_);
  Mock::VerifyAndClearExpectations(&delegate_);

  PolicyBundle expected_bundle;
  PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  expected_bundle.Get(ns).CopyFrom(expected_policy_);
  ns.component_id = kTestExtension2;
  expected_bundle.Get(ns).CopyFrom(expected_policy_);
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));

  // Now purge one of the extensions.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  // The service will start updating the components that are registered, which
  // starts by fetching policy for them.
  std::set<std::string> keep;
  keep.insert(kTestExtension2);
  service_->RegisterPolicyDomain(POLICY_DOMAIN_EXTENSIONS, keep);
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  ns.component_id = kTestExtension;
  expected_bundle.Get(ns).Clear();
  EXPECT_TRUE(service_->policy().Equals(expected_bundle));

  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys("extension-policy", &contents);
  EXPECT_EQ(1u, contents.size());
  EXPECT_TRUE(ContainsKey(contents, kTestExtension2));
  cache_->LoadAllSubkeys("unrelated", &contents);
  EXPECT_EQ(1u, contents.size());
  EXPECT_TRUE(ContainsKey(contents, "stuff"));
}

TEST_F(ComponentCloudPolicyServiceTest, UpdateCredentials) {
  // Do the same as LoadStore() but without the initial credentials.
  EXPECT_FALSE(store_.is_initialized());
  EXPECT_FALSE(service_->is_initialized());
  store_.policy_.reset(new em::PolicyData());  //  No credentials.
  EXPECT_CALL(delegate_, OnComponentCloudPolicyUpdated());
  store_.NotifyStoreLoaded();
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_TRUE(service_->is_initialized());

  // Connect the client and register an extension.
  service_->Connect(&client_, request_context_);
  EXPECT_CALL(delegate_, OnComponentCloudPolicyRefreshNeeded());
  std::set<std::string> components;
  components.insert(kTestExtension);
  service_->RegisterPolicyDomain(POLICY_DOMAIN_EXTENSIONS, components);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Send the response to the service. The response data will be rejected,
  // because the store doesn't have the updated credentials yet.
  client_.SetPolicy(PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType,
                                       kTestExtension),
                    *CreateResponse());
  service_->OnPolicyFetched(&client_);
  RunUntilIdle();

  // The policy was not verified, and no download is started.
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
