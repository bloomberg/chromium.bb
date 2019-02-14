// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/policy/cloud/cloud_policy_test_utils.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AnyNumber;
using testing::AtLeast;
using testing::AtMost;
using testing::Mock;
using testing::SaveArg;
using testing::_;

namespace {

const char kUMAReregistrationResult[] =
    "Enterprise.UserPolicyChromeOS.ReregistrationResult";

enum PolicyRequired { POLICY_NOT_REQUIRED, POLICY_REQUIRED };

}  // namespace

namespace policy {

using PolicyEnforcement = UserCloudPolicyManagerChromeOS::PolicyEnforcement;

constexpr char kEmail[] = "user@example.com";
constexpr char kTestGaiaId[] = "12345";

constexpr char kOAuth2AccessTokenData[] = R"(
    {
      "access_token": "5678",
      "expires_in": 3600
    })";
constexpr char kOAuthToken[] = "5678";
constexpr char kDMToken[] = "dmtoken123";

// UserCloudPolicyManagerChromeOS test class that can be used with different
// feature flags.
class UserCloudPolicyManagerChromeOSTest
    : public testing::TestWithParam<std::vector<base::Feature>> {
 public:
  // Note: This method has to be public, so that a pointer to it may be obtained
  // in the test.
  void MakeManagerWithPreloadedStore(const base::TimeDelta& fetch_timeout) {
    std::unique_ptr<MockCloudPolicyStore> store =
        std::make_unique<MockCloudPolicyStore>();
    store->policy_.reset(new em::PolicyData(policy_data_));
    store->policy_map_.CopyFrom(policy_map_);
    store->NotifyStoreLoaded();
    CreateManager(std::move(store), fetch_timeout,
                  PolicyEnforcement::kPolicyRequired);
    // The manager should already be initialized by this point.
    EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    InitAndConnectManager();
    EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  }

 protected:
  UserCloudPolicyManagerChromeOSTest()
      : store_(nullptr),
        external_data_manager_(nullptr),
        task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()),
        profile_(nullptr),
        signin_profile_(nullptr),
        user_manager_(new chromeos::FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(user_manager_)),
        test_signin_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_signin_url_loader_factory_)),
        test_system_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_system_url_loader_factory_)) {}

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    scoped_feature_list_.InitWithFeatures(
        GetParam() /* enabled_features */,
        std::vector<base::Feature>() /* disabled_features */);

    // The initialization path that blocks on the initial policy fetch requires
    // a signin Profile to use its URLRequestContext.
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile,
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(""), 0, std::string(), std::move(factories));
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    // Usually the signin Profile and the main Profile are separate, but since
    // the signin Profile is an OTR Profile then for this test it suffices to
    // attach it to the main Profile.
    signin_profile_ = TestingProfile::Builder().BuildIncognito(profile_);
    ASSERT_EQ(signin_profile_, chromeos::ProfileHelper::GetSigninProfile());

    RegisterLocalState(prefs_.registry());

    // Set up a policy map for testing.
    GetExpectedDefaultPolicy(&policy_map_);
    policy_map_.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>("http://chromium.org"),
                    nullptr);
    expected_bundle_.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .CopyFrom(policy_map_);

    // Create fake policy blobs to deliver to the client.
    em::DeviceRegisterResponse* register_response =
        register_blob_.mutable_register_response();
    register_response->set_device_management_token(kDMToken);

    em::CloudPolicySettings policy_proto;
    policy_proto.mutable_homepagelocation()->set_value("http://chromium.org");
    ASSERT_TRUE(
        policy_proto.SerializeToString(policy_data_.mutable_policy_value()));
    policy_data_.set_policy_type(dm_protocol::kChromeUserPolicyType);
    policy_data_.set_request_token(kDMToken);
    policy_data_.set_device_id("id987");
    policy_data_.set_username("user@example.com");
    em::PolicyFetchResponse* policy_response =
        policy_blob_.mutable_policy_response()->add_responses();
    ASSERT_TRUE(policy_data_.SerializeToString(
        policy_response->mutable_policy_data()));

    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(AnyNumber());

    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmail, kTestGaiaId);
    TestingProfile* profile =
        profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
    user_manager_->AddUserWithAffiliationAndTypeAndProfile(account_id, false,
                                                           user_type_, profile);
    user_manager_->SwitchActiveUser(account_id);
    ASSERT_TRUE(user_manager_->GetActiveUser());
  }

  void TearDown() override {
    EXPECT_EQ(fatal_error_expected_, fatal_error_encountered_);
    if (token_forwarder_)
      token_forwarder_->Shutdown();
    if (manager_) {
      manager_->RemoveObserver(&observer_);
      manager_->Shutdown();
    }
    signin_profile_ = NULL;
    profile_ = NULL;
    identity_test_env_profile_adaptor_.reset();
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
    test_system_shared_loader_factory_->Detach();
    test_signin_shared_loader_factory_->Detach();

    chromeos::DBusThreadManager::Shutdown();
  }

  void MakeManagerWithEmptyStore(const base::TimeDelta& fetch_timeout,
                                 PolicyEnforcement enforcement_type) {
    std::unique_ptr<MockCloudPolicyStore> store =
        std::make_unique<MockCloudPolicyStore>();
    EXPECT_CALL(*store, Load());
    CreateManager(std::move(store), fetch_timeout, enforcement_type);
    EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    InitAndConnectManager();
    Mock::VerifyAndClearExpectations(store_);
    EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  }

  // Issues the OAuth2 tokens and returns the device management register job
  // if the flow succeeded.
  MockDeviceManagementJob* IssueOAuthToken(bool has_request_token) {
    EXPECT_FALSE(manager_->core()->client()->is_registered());

    // Issuing this token triggers the callback of the OAuth2PolicyFetcher,
    // which triggers the registration request.
    MockDeviceManagementJob* register_request = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION, _))
        .WillOnce(device_management_service_.CreateAsyncJob(&register_request));

    if (!has_request_token) {
      GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

      network::URLLoaderCompletionStatus ok_completion_status(net::OK);
      network::ResourceResponseHead ok_response =
          network::CreateResourceResponseHead(net::HTTP_OK);
      // Issue the access token.
      EXPECT_TRUE(
          test_system_url_loader_factory_.SimulateResponseForPendingRequest(
              gaia_urls->oauth2_token_url(), ok_completion_status, ok_response,
              kOAuth2AccessTokenData));
    } else {
      // Since the refresh token is available, IdentityManager was used
      // to request the access token and not UserCloudPolicyTokenForwarder.
      // Issue the access token with the former.
      identity::ScopeSet scopes;
      scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
      scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);

      identity_test_env()
          ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
              kOAuthToken,
              base::Time::Now() +
                  base::TimeDelta::FromSeconds(3600) /*expiration*/,
              std::string() /*id_token*/, scopes);
    }

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
  void FetchPolicy(base::OnceClosure trigger_fetch, bool timeout) {
    MockDeviceManagementJob* policy_request = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
        .WillOnce(device_management_service_.CreateAsyncJob(&policy_request));
    bool is_oauth_token_passed =
        user_type_ == user_manager::UserType::USER_TYPE_CHILD &&
        base::FeatureList::IsEnabled(features::kDMServerOAuthForChildUser);
    EXPECT_CALL(device_management_service_,
                StartJob(dm_protocol::kValueRequestPolicy, "" /* gaia_token */,
                         is_oauth_token_passed ? kOAuthToken : "", kDMToken,
                         "" /* enrollment_token */, _, _))
        .Times(1);
    std::move(trigger_fetch).Run();
    ASSERT_TRUE(policy_request);
    EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
    EXPECT_TRUE(manager_->core()->client()->is_registered());

    Mock::VerifyAndClearExpectations(&device_management_service_);
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(AnyNumber());

    if (timeout) {
      manager_->ForceTimeoutForTest();
    } else {
      // Send the initial policy back. This completes the initialization flow.
      EXPECT_CALL(*store_, Store(_));
      policy_request->SendResponse(DM_STATUS_SUCCESS, policy_blob_);
      Mock::VerifyAndClearExpectations(store_);
      // Notifying that the store has cached the fetched policy completes the
      // process, and initializes the manager.
      EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
      store_->policy_map_.CopyFrom(policy_map_);
      store_->NotifyStoreLoaded();
    }
    EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_TRUE(manager_->policies().Equals(expected_bundle_));
  }

  // Required by the refresh scheduler that's created by the manager and
  // for the cleanup of URLRequestContextGetter in the |signin_profile_|.
  content::TestBrowserThreadBundle thread_bundle_;

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
  MockCloudPolicyStore* store_;  // Not owned.
  MockCloudExternalDataManager* external_data_manager_;  // Not owned.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  SchemaRegistry schema_registry_;
  std::unique_ptr<UserCloudPolicyManagerChromeOS> manager_;
  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder_;

  // Required by ProfileHelper to get the signin Profile context.
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  TestingProfile* signin_profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  user_manager::UserType user_type_ = user_manager::UserType::USER_TYPE_REGULAR;

  chromeos::FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  // This is automatically checked in TearDown() to ensure that we get a
  // fatal error iff |fatal_error_expected_| is true.
  bool fatal_error_expected_ = false;

  void CreateManager(std::unique_ptr<MockCloudPolicyStore> store,
                     const base::TimeDelta& fetch_timeout,
                     PolicyEnforcement enforcement_type) {
    store_ = store.get();
    external_data_manager_ = new MockCloudExternalDataManager;
    external_data_manager_->SetPolicyStore(store_);
    const user_manager::User* active_user = user_manager_->GetActiveUser();
    manager_.reset(new UserCloudPolicyManagerChromeOS(
        chromeos::ProfileHelper::Get()->GetProfileByUser(active_user),
        std::move(store),
        base::WrapUnique<MockCloudExternalDataManager>(external_data_manager_),
        base::FilePath(), enforcement_type, fetch_timeout,
        base::BindOnce(
            &UserCloudPolicyManagerChromeOSTest::OnFatalErrorEncountered,
            base::Unretained(this)),
        active_user->GetAccountId(), task_runner_));
    manager_->AddObserver(&observer_);
    manager_->SetSignInURLLoaderFactoryForTests(
        test_signin_shared_loader_factory_);
    manager_->SetSystemURLLoaderFactoryForTests(
        test_system_shared_loader_factory_);
    manager_->SetUserContextRefreshTokenForTests("fake-user-context-rt");
  }

  void InitAndConnectManager() {
    manager_->Init(&schema_registry_);
    manager_->Connect(&prefs_, &device_management_service_,
                      /*system_url_loader_factory=*/nullptr);
    // Create the UserCloudPolicyTokenForwarder, which fetches the access
    // token using the IdentityManager and forwards it to the
    // UserCloudPolicyManagerChromeOS. This service is automatically created
    // for regular Profiles but not for testing Profiles.
    identity::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    ASSERT_TRUE(identity_manager);
    token_forwarder_ = std::make_unique<UserCloudPolicyTokenForwarder>(
        manager_.get(), identity_test_env()->identity_manager());
    token_forwarder_->OverrideTimeForTesting(task_runner_->GetMockClock(),
                                             task_runner_->GetMockTickClock(),
                                             task_runner_);
  }

  network::TestURLLoaderFactory* test_signin_url_loader_factory() {
    return &test_signin_url_loader_factory_;
  }

  network::TestURLLoaderFactory* test_system_url_loader_factory() {
    return &test_system_url_loader_factory_;
  }

  identity::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 private:
  // Invoked when a fatal error is encountered.
  void OnFatalErrorEncountered() { fatal_error_encountered_ = true; }

  bool fatal_error_encountered_ = false;

  network::TestURLLoaderFactory test_signin_url_loader_factory_;
  network::TestURLLoaderFactory test_system_url_loader_factory_;

  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_signin_shared_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_system_shared_loader_factory_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOSTest);
};

// TODO(agawronska): Remove test instantiation with kDMServerOAuthForChildUser
// once it is enabled by default.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UserCloudPolicyManagerChromeOSTest,
    testing::Values(std::vector<base::Feature>(),
                    std::vector<base::Feature>{
                        features::kDMServerOAuthForChildUser}));

TEST_P(UserCloudPolicyManagerChromeOSTest, BlockingFirstFetch) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the policy cache is empty.
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta(), PolicyEnforcement::kServerCheckRequired));

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
  FetchPolicy(base::BindOnce(&MockDeviceManagementJob::SendResponse,
                             base::Unretained(register_request),
                             DM_STATUS_SUCCESS, register_blob_),
              false);
}

TEST_P(UserCloudPolicyManagerChromeOSTest, BlockingRefreshFetch) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // refresh fetch, when a previously cached policy and DMToken already exist.
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta::FromSeconds(1000), PolicyEnforcement::kPolicyRequired));

  // Set the initially cached data and initialize the CloudPolicyService.
  // The initial policy fetch is issued using the cached DMToken.
  store_->policy_.reset(new em::PolicyData(policy_data_));
  FetchPolicy(base::BindOnce(&MockCloudPolicyStore::NotifyStoreLoaded,
                             base::Unretained(store_)),
              false);
}

TEST_P(UserCloudPolicyManagerChromeOSTest, SynchronousLoadWithEmptyStore) {
  // Tests the initialization of a manager who requires policy, but who
  // has no policy stored on disk. The manager should abort and exit the
  // session.
  fatal_error_expected_ = true;
  std::unique_ptr<MockCloudPolicyStore> store =
      std::make_unique<MockCloudPolicyStore>();
  // Tell the store it couldn't load data.
  store->NotifyStoreError();
  CreateManager(std::move(store), base::TimeDelta(),
                PolicyEnforcement::kPolicyRequired);
  InitAndConnectManager();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
}

TEST_P(UserCloudPolicyManagerChromeOSTest, BlockingFetchStoreError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the initial store load fails.
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta(), PolicyEnforcement::kServerCheckRequired));

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
  FetchPolicy(base::BindOnce(&MockDeviceManagementJob::SendResponse,
                             base::Unretained(register_request),
                             DM_STATUS_SUCCESS, register_blob_),
              false);
}

TEST_P(UserCloudPolicyManagerChromeOSTest, BlockingFetchOAuthError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the OAuth2 token fetch fails. This should result in a
  // fatal error.
  fatal_error_expected_ = true;
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta(), PolicyEnforcement::kServerCheckRequired));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  // The PolicyOAuth2TokenFetcher posts delayed retries on some errors. This
  // data will make it fail immediately.

  EXPECT_TRUE(
      test_system_url_loader_factory()->SimulateResponseForPendingRequest(
          GaiaUrls::GetInstance()->oauth2_token_url(),
          network::URLLoaderCompletionStatus(net::OK),
          network::CreateResourceResponseHead(net::HTTP_BAD_REQUEST),
          "Error=BadAuthentication"));

  // Server check failed, so profile should not be initialized.
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_P(UserCloudPolicyManagerChromeOSTest, BlockingFetchRegisterError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the device management registration fails.
  fatal_error_expected_ = true;
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta(), PolicyEnforcement::kServerCheckRequired));

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
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get())).Times(0);
  register_request->SendResponse(DM_STATUS_TEMPORARY_UNAVAILABLE,
                                 em::DeviceManagementResponse());
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_P(UserCloudPolicyManagerChromeOSTest, BlockingFetchPolicyFetchError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the policy fetch request fails.
  fatal_error_expected_ = true;
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta(), PolicyEnforcement::kServerCheckRequired));

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
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
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
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get())).Times(0);
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  policy_request->SendResponse(DM_STATUS_TEMPORARY_UNAVAILABLE,
                               em::DeviceManagementResponse());
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
}

TEST_P(UserCloudPolicyManagerChromeOSTest,
       NoCacheButPolicyExpectedRegistrationError) {
  // Tests the case where we have no local policy and the policy fetch
  // request fails, but we think we should have policy - this covers the
  // situation where local policy cache is lost due to disk corruption and
  // we can't access the server.
  fatal_error_expected_ = true;
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta::Max(), PolicyEnforcement::kPolicyRequired));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_request = IssueOAuthToken(false);
  ASSERT_TRUE(register_request);

  // Make the registration attempt fail.
  register_request->SendResponse(DM_STATUS_TEMPORARY_UNAVAILABLE,
                                 register_blob_);
  Mock::VerifyAndClearExpectations(&device_management_service_);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());
}

TEST_P(UserCloudPolicyManagerChromeOSTest, NoCacheButPolicyExpectedFetchError) {
  // Tests the case where we have no local policy and the policy fetch
  // request fails, but we think we should have policy - this covers the
  // situation where local policy cache is lost due to disk corruption and
  // we can't access the server.
  fatal_error_expected_ = true;
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta::Max(), PolicyEnforcement::kPolicyRequired));

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
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
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
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  policy_request->SendResponse(DM_STATUS_TEMPORARY_UNAVAILABLE,
                               em::DeviceManagementResponse());
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
}

TEST_P(UserCloudPolicyManagerChromeOSTest, NonBlockingFirstFetch) {
  // Tests the first policy fetch request by a Profile that isn't managed.
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta(), PolicyEnforcement::kPolicyOptional));

  // Initialize the CloudPolicyService without any stored data. Since the
  // manager is not waiting for the initial fetch, it will become initialized
  // once the store is loaded.
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

  AccountInfo account_info =
      identity_test_env()->MakePrimaryAccountAvailable(kEmail);
  EXPECT_TRUE(
      identity_test_env()->identity_manager()->HasAccountWithRefreshToken(
          account_info.account_id));

  // That should have notified the manager, which now issues the request for the
  // policy oauth token.
  MockDeviceManagementJob* register_request = IssueOAuthToken(true);
  ASSERT_TRUE(register_request);
  register_request->SendResponse(DM_STATUS_SUCCESS, register_blob_);

  // The refresh scheduler takes care of the initial fetch for unmanaged users.
  // Running the task runner issues the initial fetch.
  FetchPolicy(
      base::BindOnce(&base::TestMockTimeTaskRunner::RunUntilIdle, task_runner_),
      false);
}

TEST_P(UserCloudPolicyManagerChromeOSTest, BlockingRefreshFetchWithTimeout) {
  // Tests the case where a profile has policy, but the refresh policy fetch
  // fails (times out) - ensures that we don't mark the profile as initialized
  // until after the timeout.
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta::FromSeconds(1000), PolicyEnforcement::kPolicyRequired));

  // Set the initially cached data and initialize the CloudPolicyService.
  // The initial policy fetch is issued using the cached DMToken.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->policy_.reset(new em::PolicyData(policy_data_));
  store_->policy_map_.CopyFrom(policy_map_);

  // Mock out the initial policy fetch and have it trigger a timeout.
  FetchPolicy(base::BindOnce(&MockCloudPolicyStore::NotifyStoreLoaded,
                             base::Unretained(store_)),
              true);
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_P(UserCloudPolicyManagerChromeOSTest, SynchronousLoadWithPreloadedStore) {
  // Tests the initialization of a manager with non-blocking initial policy
  // fetch, when a previously cached policy and DMToken are already loaded
  // before the manager is constructed (this simulates synchronously
  // initializing a profile during a crash restart). The manager gets
  // initialized straight away after the construction.
  MakeManagerWithPreloadedStore(base::TimeDelta());
  EXPECT_TRUE(manager_->policies().Equals(expected_bundle_));
}

TEST_P(UserCloudPolicyManagerChromeOSTest, TestLifetimeReportingRegular) {
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta::FromSeconds(1000), PolicyEnforcement::kPolicyRequired));

  store_->NotifyStoreLoaded();

  em::DeviceManagementRequest register_request;

  EXPECT_CALL(device_management_service_,
              StartJob(dm_protocol::kValueRequestRegister, _, _, _, _, _, _))
      .Times(AtMost(1))
      .WillOnce(SaveArg<6>(&register_request));

  MockDeviceManagementJob* register_job = IssueOAuthToken(false);
  ASSERT_TRUE(register_job);
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(register_request.has_register_request());
  ASSERT_TRUE(register_request.register_request().has_lifetime());
  ASSERT_EQ(em::DeviceRegisterRequest::LIFETIME_INDEFINITE,
            register_request.register_request().lifetime());
}

TEST_P(UserCloudPolicyManagerChromeOSTest, TestLifetimeReportingEphemeralUser) {
  user_manager_->set_current_user_ephemeral(true);

  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta::FromSeconds(1000), PolicyEnforcement::kPolicyRequired));

  store_->NotifyStoreLoaded();

  em::DeviceManagementRequest register_request;

  EXPECT_CALL(device_management_service_,
              StartJob(dm_protocol::kValueRequestRegister, _, _, _, _, _, _))
      .Times(AtMost(1))
      .WillOnce(SaveArg<6>(&register_request));

  MockDeviceManagementJob* register_job = IssueOAuthToken(false);
  ASSERT_TRUE(register_job);

  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(register_request.has_register_request());
  ASSERT_TRUE(register_request.register_request().has_lifetime());
  ASSERT_EQ(em::DeviceRegisterRequest::LIFETIME_EPHEMERAL_USER,
            register_request.register_request().lifetime());
}

TEST_P(UserCloudPolicyManagerChromeOSTest, TestHasAppInstallEventLogUploader) {
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta(), PolicyEnforcement::kPolicyRequired));
  EXPECT_TRUE(manager_->GetAppInstallEventLogUploader());
}

TEST_P(UserCloudPolicyManagerChromeOSTest, Reregistration) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the policy cache is empty.
  fatal_error_expected_ = true;
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta(), PolicyEnforcement::kServerCheckRequired));
  base::HistogramTester histogram_tester;

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_job = IssueOAuthToken(false);
  ASSERT_TRUE(register_job);

  // Register.
  MockDeviceManagementJob* policy_job = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .WillRepeatedly(device_management_service_.CreateAsyncJob(&policy_job));
  register_job->SendResponse(DM_STATUS_SUCCESS, register_blob_);

  // Validate registered state.
  ASSERT_TRUE(policy_job);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_TRUE(manager_->core()->client()->is_registered());
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());

  // Simulate policy fetch fail (error code 410), which should trigger the
  // re-registration flow (FetchPolicyOAuthToken()).
  MockDeviceManagementJob* reregister_job = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&reregister_job));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get())).Times(0);
  policy_job->SendResponse(DM_STATUS_SERVICE_DEVICE_NOT_FOUND,
                           em::DeviceManagementResponse());
  histogram_tester.ExpectUniqueSample(kUMAReregistrationResult, 0, 1);

  // Copy register request (used to check correct re-registration parameters are
  // submitted).
  em::DeviceManagementRequest register_request;
  EXPECT_CALL(device_management_service_,
              StartJob(dm_protocol::kValueRequestRegister, _, _, _, _, _, _))
      .WillOnce(SaveArg<6>(&register_request));

  // Simulate OAuth token fetch.
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  network::URLLoaderCompletionStatus ok_completion_status(net::OK);
  network::ResourceResponseHead ok_response =
      network::CreateResourceResponseHead(net::HTTP_OK);
  EXPECT_TRUE(
      test_system_url_loader_factory()->SimulateResponseForPendingRequest(
          gaia_urls->oauth2_token_url(), ok_completion_status, ok_response,
          kOAuth2AccessTokenData));

  // Validate that re-registration sends the correct parameters.
  EXPECT_TRUE(register_request.register_request().reregister());
  EXPECT_EQ(kDMToken,
            register_request.register_request().reregistration_dm_token());

  // Validate re-registration state.
  ASSERT_TRUE(reregister_job);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());

  // Validate successful re-registration.
  reregister_job->SendResponse(DM_STATUS_SUCCESS, register_blob_);
  ASSERT_TRUE(policy_job);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_TRUE(manager_->core()->client()->is_registered());
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
  histogram_tester.ExpectBucketCount(kUMAReregistrationResult, 0, 1);
  histogram_tester.ExpectBucketCount(kUMAReregistrationResult, 1, 1);
  histogram_tester.ExpectBucketCount(kUMAReregistrationResult, 2, 0);
  histogram_tester.ExpectTotalCount(kUMAReregistrationResult, 2);
}

TEST_P(UserCloudPolicyManagerChromeOSTest, ReregistrationFails) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the policy cache is empty.
  fatal_error_expected_ = true;
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta(), PolicyEnforcement::kServerCheckRequired));
  base::HistogramTester histogram_tester;

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_job = IssueOAuthToken(false);
  ASSERT_TRUE(register_job);

  // Register.
  MockDeviceManagementJob* policy_job = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .WillRepeatedly(device_management_service_.CreateAsyncJob(&policy_job));
  register_job->SendResponse(DM_STATUS_SUCCESS, register_blob_);

  // Validate registered state.
  ASSERT_TRUE(policy_job);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_TRUE(manager_->core()->client()->is_registered());
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());

  // Simulate policy fetch fail (error code 410), which should trigger the
  // re-registration flow (FetchPolicyOAuthToken()).
  MockDeviceManagementJob* reregister_job = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&reregister_job));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get())).Times(0);
  policy_job->SendResponse(DM_STATUS_SERVICE_DEVICE_NOT_FOUND,
                           em::DeviceManagementResponse());
  histogram_tester.ExpectUniqueSample(kUMAReregistrationResult, 0, 1);

  // Simulate OAuth token fetch.
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  network::URLLoaderCompletionStatus ok_completion_status(net::OK);
  network::ResourceResponseHead ok_response =
      network::CreateResourceResponseHead(net::HTTP_OK);
  EXPECT_TRUE(
      test_system_url_loader_factory()->SimulateResponseForPendingRequest(
          gaia_urls->oauth2_token_url(), ok_completion_status, ok_response,
          kOAuth2AccessTokenData));

  // Validate re-registration state.
  ASSERT_TRUE(reregister_job);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());

  // Validate unsuccessful re-registration.
  reregister_job->SendResponse(DM_STATUS_TEMPORARY_UNAVAILABLE, register_blob_);
  ASSERT_TRUE(policy_job);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());
  EXPECT_TRUE(user_manager_->GetActiveUser()->force_online_signin());
  histogram_tester.ExpectBucketCount(kUMAReregistrationResult, 0, 1);
  histogram_tester.ExpectBucketCount(kUMAReregistrationResult, 1, 0);
  histogram_tester.ExpectBucketCount(kUMAReregistrationResult, 2, 1);
  histogram_tester.ExpectTotalCount(kUMAReregistrationResult, 2);
}

// Tests UserCloudPolicyManagerChromeOS for child user.
class UserCloudPolicyManagerChromeOSChildTest
    : public UserCloudPolicyManagerChromeOSTest {
 public:
  // Issues OAuthToken for device management scopes using OAuth2TokenService.
  void IssueOAuthTokenWithTokenService(base::TimeDelta token_lifetime) {
    identity::ScopeSet scopes;
    scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
    scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            kOAuthToken, task_runner_->Now() + token_lifetime,
            std::string() /*id_token*/, scopes);
  }

 protected:
  UserCloudPolicyManagerChromeOSChildTest() {
    user_type_ = user_manager::UserType::USER_TYPE_CHILD;
  }
  ~UserCloudPolicyManagerChromeOSChildTest() override = default;

  // UserCloudPolicyManagerChromeOSTest:
  void SetUp() override {
    UserCloudPolicyManagerChromeOSTest::SetUp();
    identity_test_env()->MakePrimaryAccountAvailable(kEmail);
  }

  // Sets the initially cached data and initializes the CloudPolicyService.
  void LoadStoreWithCachedData() {
    store_->policy_ = std::make_unique<em::PolicyData>(policy_data_);
    store_->policy_map_.CopyFrom(policy_map_);
    store_->NotifyStoreLoaded();
    EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOSChildTest);
};

// TODO(agawronska): Remove test instantiation with kDMServerOAuthForChildUser
// once it is enabled by default.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UserCloudPolicyManagerChromeOSChildTest,
    testing::Values(std::vector<base::Feature>{
        features::kDMServerOAuthForChildUser}));

TEST_P(UserCloudPolicyManagerChromeOSChildTest, RefreshFetchDoesNotBlock) {
  // Tests the profile initialization is not blocked on policy refresh.
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta::FromSeconds(0), PolicyEnforcement::kPolicyRequired));
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  LoadStoreWithCachedData();
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

TEST_P(UserCloudPolicyManagerChromeOSChildTest, RefreshSchedulerStart) {
  // Tests that refresh scheduler is started after OAuth token is available.
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta::FromSeconds(0), PolicyEnforcement::kPolicyRequired));
  LoadStoreWithCachedData();
  EXPECT_FALSE(manager_->core()->refresh_scheduler());

  IssueOAuthTokenWithTokenService(base::TimeDelta::FromSeconds(3600));

  EXPECT_TRUE(manager_->core()->refresh_scheduler());
}

TEST_P(UserCloudPolicyManagerChromeOSChildTest, RefreshScheduler) {
  // Tests that refresh schedule isn't affected by periodic OAuth token updates.
  ASSERT_NO_FATAL_FAILURE(MakeManagerWithEmptyStore(
      base::TimeDelta::FromSeconds(0), PolicyEnforcement::kPolicyRequired));
  LoadStoreWithCachedData();

  // This starts refresh scheduler.
  const base::TimeDelta token_lifetime = base::TimeDelta::FromMinutes(50);
  IssueOAuthTokenWithTokenService(token_lifetime);

  // First refresh is scheduled with delay of 0s - let it execute.
  FetchPolicy(
      base::BindOnce(&base::TestMockTimeTaskRunner::RunUntilIdle, task_runner_),
      false);

  // Assert that the initial delay (right now 3h) is at least |iterations| times
  // longer than token lifetime. If default delay changes and is lesser the rest
  // of the test will work incorrectly and should be updated.
  const int iterations = 3;
  base::TimeDelta refresh_delay = base::TimeDelta::FromMilliseconds(
      manager_->core()->refresh_scheduler()->GetActualRefreshDelay());
  ASSERT_GT(refresh_delay, iterations * token_lifetime);

  // Advancing the clock will trigger delivery of new tokens. It should not
  // trigger policy refresh nor affect scheduled refresh.
  for (int i = 0; i < iterations; ++i) {
    task_runner_->FastForwardBy(token_lifetime);
    refresh_delay -= token_lifetime;
    IssueOAuthTokenWithTokenService(token_lifetime);
  }

  // Advance the clock by the remaining time to get scheduled policy refresh.
  FetchPolicy(base::BindOnce(&base::TestMockTimeTaskRunner::FastForwardBy,
                             task_runner_, refresh_delay),
              false);
}

}  // namespace policy
