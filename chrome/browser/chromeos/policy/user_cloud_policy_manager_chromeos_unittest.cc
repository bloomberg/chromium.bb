// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/cloud_policy_service.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_store.h"
#include "chrome/browser/policy/cloud/mock_device_management_service.h"
#include "chrome/browser/policy/cloud/resource_cache.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::_;

namespace policy {

namespace {

const char kOAuthTokenCookie[] = "oauth_token=1234";

const char kOAuth2TokenPairData[] =
    "{"
    "  \"refresh_token\": \"1234\","
    "  \"access_token\": \"5678\","
    "  \"expires_in\": 3600"
    "}";

const char kOAuth2AccessTokenData[] =
    "{"
    "  \"access_token\": \"5678\","
    "  \"expires_in\": 3600"
    "}";

}  // namespace

class UserCloudPolicyManagerChromeOSTest : public testing::Test {
 protected:
  UserCloudPolicyManagerChromeOSTest()
      : ui_thread_(content::BrowserThread::UI, &loop_),
        io_thread_(content::BrowserThread::IO, &loop_),
        store_(NULL),
        profile_(NULL),
        signin_profile_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // The initialization path that blocks on the initial policy fetch requires
    // a signin Profile to use its URLRequestContext.
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile, UTF8ToUTF16("testing_profile"), 0);
    signin_profile_ = profile_manager_->CreateTestingProfile("signin_profile");
    signin_profile_->set_incognito(true);
    // Usually the signin Profile and the main Profile are separate, but since
    // the signin Profile is an OTR Profile then for this test it suffices to
    // attach it to the main Profile.
    profile_->SetOffTheRecordProfile(signin_profile_);
    signin_profile_->SetOriginalProfile(profile_);
    signin_profile_->CreateRequestContext();
    ASSERT_EQ(signin_profile_, chromeos::ProfileHelper::GetSigninProfile());

    chrome::RegisterLocalState(prefs_.registry());

    // Set up a policy map for testing.
    policy_map_.Set("HomepageLocation",
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    base::Value::CreateStringValue("http://chromium.org"));
    expected_bundle_.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .CopyFrom(policy_map_);

    // Create fake policy blobs to deliver to the client.
    em::DeviceRegisterResponse* register_response =
        register_blob_.mutable_register_response();
    register_response->set_device_management_token("dmtoken123");

    em::CloudPolicySettings policy_proto;
    policy_proto.mutable_homepagelocation()->set_value("http://chromium.org");
    ASSERT_TRUE(
        policy_proto.SerializeToString(policy_data_.mutable_policy_value()));
    policy_data_.set_policy_type(dm_protocol::kChromeUserPolicyType);
    policy_data_.set_request_token("dmtoken123");
    policy_data_.set_device_id("id987");
    em::PolicyFetchResponse* policy_response =
        policy_blob_.mutable_policy_response()->add_response();
    ASSERT_TRUE(policy_data_.SerializeToString(
        policy_response->mutable_policy_data()));

    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(AnyNumber());
  }

  virtual void TearDown() OVERRIDE {
    if (manager_) {
      manager_->RemoveObserver(&observer_);
      manager_->Shutdown();
    }
    signin_profile_->ResetRequestContext();
  }

  void CreateManager(bool wait_for_fetch) {
    store_ = new MockCloudPolicyStore();
    EXPECT_CALL(*store_, Load());
    manager_.reset(new UserCloudPolicyManagerChromeOS(
        scoped_ptr<CloudPolicyStore>(store_),
        scoped_ptr<ResourceCache>(),
        wait_for_fetch));
    manager_->Init();
    manager_->AddObserver(&observer_);
    manager_->Connect(&prefs_, &device_management_service_, NULL,
                      USER_AFFILIATION_NONE);
    Mock::VerifyAndClearExpectations(store_);
    EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());

    if (!wait_for_fetch) {
      // Create the UserCloudPolicyTokenForwarder, which forwards the refresh
      // token from the TokenService to the UserCloudPolicyManagerChromeOS.
      // This service is automatically created for regular Profiles but not for
      // testing Profiles.
      TokenService* token_service =
          TokenServiceFactory::GetForProfile(profile_);
      ASSERT_TRUE(token_service);
      token_forwarder_.reset(
          new UserCloudPolicyTokenForwarder(manager_.get(), token_service));
    }
  }

  // Expects a pending URLFetcher for the |expected_url|, and returns it with
  // prepared to deliver a response to its delegate.
  net::TestURLFetcher* PrepareOAuthFetcher(const std::string& expected_url) {
    net::TestURLFetcher* fetcher = test_url_fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    if (!fetcher)
      return NULL;
    EXPECT_TRUE(fetcher->delegate());
    EXPECT_TRUE(StartsWithASCII(fetcher->GetOriginalURL().spec(),
                                expected_url,
                                true));
    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_response_code(200);
    fetcher->set_status(net::URLRequestStatus());
    return fetcher;
  }

  // Issues the OAuth2 tokens and returns the device management register job
  // if the flow succeeded.
  MockDeviceManagementJob* IssueOAuthToken(bool has_request_token) {
    EXPECT_FALSE(manager_->core()->client()->is_registered());

    GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
    net::TestURLFetcher* fetcher = NULL;

    if (!has_request_token) {
      // Issue the oauth_token cookie first.
      fetcher = PrepareOAuthFetcher(gaia_urls->client_login_to_oauth2_url());
      if (!fetcher)
        return NULL;
      net::ResponseCookies cookies;
      cookies.push_back(kOAuthTokenCookie);
      fetcher->set_cookies(cookies);
      fetcher->delegate()->OnURLFetchComplete(fetcher);

      // Issue the refresh token.
      fetcher = PrepareOAuthFetcher(gaia_urls->oauth2_token_url());
      if (!fetcher)
        return NULL;
      fetcher->SetResponseString(kOAuth2TokenPairData);
      fetcher->delegate()->OnURLFetchComplete(fetcher);
    }

    // Issue the access token.
    fetcher = PrepareOAuthFetcher(gaia_urls->oauth2_token_url());
    if (!fetcher)
      return NULL;
    fetcher->SetResponseString(kOAuth2AccessTokenData);

    // Issuing this token triggers the callback of the OAuth2PolicyFetcher,
    // which triggers the registration request.
    MockDeviceManagementJob* register_request = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION))
        .WillOnce(device_management_service_.CreateAsyncJob(&register_request));
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    EXPECT_TRUE(register_request);
    EXPECT_FALSE(manager_->core()->client()->is_registered());

    Mock::VerifyAndClearExpectations(&device_management_service_);
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(AnyNumber());

    return register_request;
  }

  // Expects a policy fetch request to be issued after invoking |trigger_fetch|.
  // This method replies to that fetch request and verifies that the manager
  // handled the response.
  void FetchPolicy(const base::Closure& trigger_fetch) {
    MockDeviceManagementJob* policy_request = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH))
        .WillOnce(device_management_service_.CreateAsyncJob(&policy_request));
    trigger_fetch.Run();
    ASSERT_TRUE(policy_request);
    EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
    EXPECT_TRUE(manager_->core()->client()->is_registered());

    Mock::VerifyAndClearExpectations(&device_management_service_);
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(AnyNumber());

    // Send the initial policy back. This completes the initialization flow.
    EXPECT_CALL(*store_, Store(_));
    policy_request->SendResponse(DM_STATUS_SUCCESS, policy_blob_);
    Mock::VerifyAndClearExpectations(store_);

    // Notifying that the store is has cached the fetched policy completes the
    // process, and initializes the manager.
    EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
    store_->policy_map_.CopyFrom(policy_map_);
    store_->NotifyStoreLoaded();
    EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_TRUE(manager_->policies().Equals(expected_bundle_));
  }

  // Required by the refresh scheduler that's created by the manager.
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  // Required to cleanup the URLRequestContextGetter of the |signin_profile_|.
  content::TestBrowserThread io_thread_;

  // Convenience policy objects.
  em::PolicyData policy_data_;
  em::DeviceManagementResponse register_blob_;
  em::DeviceManagementResponse policy_blob_;
  PolicyMap policy_map_;
  PolicyBundle expected_bundle_;

  // Policy infrastructure.
  net::TestURLFetcherFactory test_url_fetcher_factory_;
  TestingPrefServiceSimple prefs_;
  MockConfigurationPolicyObserver observer_;
  MockDeviceManagementService device_management_service_;
  MockCloudPolicyStore* store_;
  scoped_ptr<UserCloudPolicyManagerChromeOS> manager_;
  scoped_ptr<UserCloudPolicyTokenForwarder> token_forwarder_;

  // Required by ProfileHelper to get the signin Profile context.
  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  TestingProfile* signin_profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOSTest);
};

TEST_F(UserCloudPolicyManagerChromeOSTest, BlockingFirstFetch) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the policy cache is empty.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_request = IssueOAuthToken(false);
  ASSERT_TRUE(register_request);

  // Reply with a valid registration response. This triggers the initial policy
  // fetch.
  FetchPolicy(base::Bind(&MockDeviceManagementJob::SendResponse,
                         base::Unretained(register_request),
                         DM_STATUS_SUCCESS, register_blob_));
}

TEST_F(UserCloudPolicyManagerChromeOSTest, BlockingRefreshFetch) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when a previously cached policy and DMToken already exist.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true));

  // Set the initially cached data and initialize the CloudPolicyService.
  // The initial policy fetch is issued using the cached DMToken.
  store_->policy_.reset(new em::PolicyData(policy_data_));
  FetchPolicy(base::Bind(&MockCloudPolicyStore::NotifyStoreLoaded,
                         base::Unretained(store_)));
}

TEST_F(UserCloudPolicyManagerChromeOSTest, BlockingFetchStoreError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the initial store load fails.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreError();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_request = IssueOAuthToken(false);
  ASSERT_TRUE(register_request);

  // Reply with a valid registration response. This triggers the initial policy
  // fetch.
  FetchPolicy(base::Bind(&MockDeviceManagementJob::SendResponse,
                         base::Unretained(register_request),
                         DM_STATUS_SUCCESS, register_blob_));
}

TEST_F(UserCloudPolicyManagerChromeOSTest, BlockingFetchOAuthError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the OAuth2 token fetch fails.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will initialize with no policy after the token fetcher fails.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));

  // The PolicyOAuth2TokenFetcher posts delayed retries on some errors. This
  // data will make it fail immediately.
  net::TestURLFetcher* fetcher = PrepareOAuthFetcher(
      GaiaUrls::GetInstance()->client_login_to_oauth2_url());
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(400);
  fetcher->SetResponseString("Error=BadAuthentication");
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(UserCloudPolicyManagerChromeOSTest, BlockingFetchRegisterError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the device management registration fails.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreError();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_request = IssueOAuthToken(false);
  ASSERT_TRUE(register_request);

  // Now make it fail.
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  register_request->SendResponse(DM_STATUS_TEMPORARY_UNAVAILABLE,
                                 em::DeviceManagementResponse());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(UserCloudPolicyManagerChromeOSTest, BlockingFetchPolicyFetchError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the policy fetch request fails.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_request = IssueOAuthToken(false);
  ASSERT_TRUE(register_request);

  // Reply with a valid registration response. This triggers the initial policy
  // fetch.
  MockDeviceManagementJob* policy_request = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH))
      .WillOnce(device_management_service_.CreateAsyncJob(&policy_request));
  register_request->SendResponse(DM_STATUS_SUCCESS, register_blob_);
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(policy_request);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_TRUE(manager_->core()->client()->is_registered());

  // Make the policy fetch fail. The observer gets 2 notifications: one from the
  // RefreshPolicies callback, and another from the OnClientError callback.
  // A single notification suffices for this edge case, but this behavior is
  // also correct and makes the implementation simpler.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get())).Times(AtLeast(1));
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  policy_request->SendResponse(DM_STATUS_TEMPORARY_UNAVAILABLE,
                               em::DeviceManagementResponse());
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
}

TEST_F(UserCloudPolicyManagerChromeOSTest, NonBlockingFirstFetch) {
  // Tests the first policy fetch request by a Profile that isn't managed.
  ASSERT_NO_FATAL_FAILURE(CreateManager(false));

  // Initialize the CloudPolicyService without any stored data. Since the
  // manager is not waiting for the initial fetch, it will become initialized
  // once the store is ready.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // The manager is waiting for the refresh token, and hasn't started any
  // fetchers.
  EXPECT_FALSE(test_url_fetcher_factory_.GetFetcherByID(0));

  // Set a fake refresh token at the TokenService.
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  ASSERT_TRUE(token_service);
  GaiaAuthConsumer::ClientOAuthResult tokens("refresh", "access", 3600);
  EXPECT_FALSE(token_service->HasOAuthLoginToken());
  token_service->UpdateCredentialsWithOAuth2(tokens);
  EXPECT_TRUE(token_service->HasOAuthLoginToken());

  // That should have notified the manager, which now issues the request for the
  // policy oauth token.
  MockDeviceManagementJob* register_request = IssueOAuthToken(true);
  ASSERT_TRUE(register_request);
  register_request->SendResponse(DM_STATUS_SUCCESS, register_blob_);

  // The refresh scheduler takes care of the initial fetch for unmanaged users.
  // It posts a delayed task with 0ms delay in this case, so spinning the loop
  // issues the initial fetch.
  base::RunLoop loop;
  FetchPolicy(
      base::Bind(&base::RunLoop::RunUntilIdle, base::Unretained(&loop)));
}

TEST_F(UserCloudPolicyManagerChromeOSTest, NonBlockingRefreshFetch) {
  // Tests a non-blocking initial policy fetch for a Profile that already has
  // a cached DMToken.
  ASSERT_NO_FATAL_FAILURE(CreateManager(false));

  // Set the initially cached data and initialize the CloudPolicyService.
  // The initial policy fetch is issued using the cached DMToken.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->policy_.reset(new em::PolicyData(policy_data_));
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(manager_->core()->client()->is_registered());

  // The refresh scheduler takes care of the initial fetch for unmanaged users.
  // It posts a delayed task with 0ms delay in this case, so spinning the loop
  // issues the initial fetch.
  base::RunLoop loop;
  FetchPolicy(
      base::Bind(&base::RunLoop::RunUntilIdle, base::Unretained(&loop)));
}

}  // namespace policy
