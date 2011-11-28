// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/dbus/mock_dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/mock_session_manager_client.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

namespace em = enterprise_management;

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;
using content::BrowserThread;

const char kTrue[] = "true";
const char kDomain[] = "domain.com";
const char kUsername[] = "user@domain.com";
const char kUsernameOtherDomain[] = "user@other.com";
const char kAttributeOwned[] = "enterprise.owned";
const char kAttributeOwner[] = "enterprise.user";

const char kOAuthTokenCookie[] = "oauth_token=1234";
const char kOAuthGetAccessTokenData[] =
    "oauth_token=1234&oauth_token_secret=1234";
const char kOAuthServiceTokenData[] =
    "wrap_access_token=1234&wrap_access_token_expires_in=123456789";

const char kDMServer[] = "http://server/device_management";
const char kDMRegisterRequest[] =
    "http://server/device_management?request=register";
const char kDMPolicyRequest[] =
    "http://server/device_management?request=policy";

const char kDMToken[] = "1234";

ACTION_P(MockSessionManagerClientPolicyCallback, policy) {
  arg0.Run(policy);
}

template<typename TESTBASE>
class LoginUtilsTestBase : public TESTBASE,
                           public LoginUtils::Delegate,
                           public LoginStatusConsumer {
 public:
  // Initialization here is important. The UI thread gets the test's
  // message loop, as does the file thread (which never actually gets
  // started - so this is a way to fake multiple threads on a single
  // test thread).  The IO thread does not get the message loop set,
  // and is never started.  This is necessary so that we skip various
  // bits of initialization that get posted to the IO thread.  We do
  // however, at one point in the test, temporarily set the message
  // loop for the IO thread.
  LoginUtilsTestBase()
      : loop_(MessageLoop::TYPE_IO),
        browser_process_(
            static_cast<TestingBrowserProcess*>(g_browser_process)),
        local_state_(browser_process_),
        ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_),
        io_thread_(content::BrowserThread::IO),
        io_thread_state_(local_state_.Get(), NULL, NULL),
        prepared_profile_(NULL) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    // BrowserPolicyConnector makes the UserPolicyCache read relative to this
    // path. Make sure it's in a clean state.
    PathService::Override(chrome::DIR_USER_DATA, scoped_temp_dir_.path());

    CommandLine* command_line = CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kEnableDevicePolicy);
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl, kDMServer);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");

    local_state_.Get()->RegisterStringPref(prefs::kApplicationLocale, "");

    browser_process_->SetIOThread(&io_thread_state_);

    DBusThreadManager::InitializeForTesting(&dbus_thread_manager_);

    MockSessionManagerClient* session_managed_client =
        dbus_thread_manager_.mock_session_manager_client();
    EXPECT_CALL(*session_managed_client, RetrievePolicy(_))
        .WillRepeatedly(MockSessionManagerClientPolicyCallback(""));

    CrosLibrary::TestApi* test_api = CrosLibrary::Get()->GetTestApi();
    ASSERT_TRUE(test_api);

    MockLibraryLoader* loader = new MockLibraryLoader();
    ON_CALL(*loader, Load(_)).WillByDefault(Return(true));
    test_api->SetLibraryLoader(loader, true);

    cryptohome_ = new MockCryptohomeLibrary();
    EXPECT_CALL(*cryptohome_, InstallAttributesIsReady())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*cryptohome_, InstallAttributesIsInvalid())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*cryptohome_, InstallAttributesIsFirstInstall())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*cryptohome_, TpmIsEnabled())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*cryptohome_, IsMounted())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*cryptohome_, InstallAttributesSet(kAttributeOwned, kTrue))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*cryptohome_, InstallAttributesSet(kAttributeOwner,
                                                   kUsername))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*cryptohome_, InstallAttributesFinalize())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*cryptohome_, InstallAttributesGet(kAttributeOwned, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(kTrue),
                              Return(true)));
    EXPECT_CALL(*cryptohome_, InstallAttributesGet(kAttributeOwner, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(kUsername),
                              Return(true)));
    test_api->SetCryptohomeLibrary(cryptohome_, true);

    browser_process_->SetProfileManager(
        new ProfileManagerWithoutInit(scoped_temp_dir_.path()));
    connector_ = browser_process_->browser_policy_connector();
    connector_->Init();

    loop_.RunAllPending();
  }

  virtual void TearDown() OVERRIDE {
    loop_.RunAllPending();
    {
      // chrome_browser_net::Predictor usually skips its shutdown routines on
      // unit_tests, but does the full thing when
      // g_browser_process->profile_manager() is valid during initialization.
      // Run a task on a temporary BrowserThread::IO that allows skipping
      // these routines.
      //
      // It is important to not have a fake message loop on the IO
      // thread for the whole test, see comment on LoginUtilsTestBase
      // constructor for details.
      io_thread_.DeprecatedSetMessageLoop(&loop_);
      loop_.PostTask(FROM_HERE,
                     base::Bind(&LoginUtilsTestBase::TearDownOnIO,
                                base::Unretained(this)));
      loop_.RunAllPending();
      io_thread_.DeprecatedSetMessageLoop(NULL);
    }

    // These trigger some tasks that have to run while BrowserThread::UI
    // exists.
    connector_ = NULL;
    browser_process_->SetBrowserPolicyConnector(NULL);
    browser_process_->SetProfileManager(NULL);
    loop_.RunAllPending();
  }

  void TearDownOnIO() {
    std::vector<Profile*> profiles =
        browser_process_->profile_manager()->GetLoadedProfiles();
    for (size_t i = 0; i < profiles.size(); ++i) {
      chrome_browser_net::Predictor* predictor =
          profiles[i]->GetNetworkPredictor();
      if (predictor) {
        predictor->EnablePredictorOnIOThread(false);
        predictor->Shutdown();
      }
    }
  }

  virtual void OnProfilePrepared(Profile* profile) OVERRIDE {
    EXPECT_FALSE(prepared_profile_);
    prepared_profile_ = profile;
  }

  virtual void OnLoginFailure(const LoginFailure& error) OVERRIDE {
    FAIL() << "OnLoginFailure not expected";
  }

  virtual void OnLoginSuccess(const std::string& username,
                              const std::string& password,
                              const GaiaAuthConsumer::ClientLoginResult& creds,
                              bool pending_requests,
                              bool using_oauth) OVERRIDE {
    FAIL() << "OnLoginSuccess not expected";
  }

  void LockDevice(const std::string& username) {
    EXPECT_CALL(*cryptohome_, InstallAttributesIsFirstInstall())
        .WillOnce(Return(true))
        .WillRepeatedly(Return(false));
    EXPECT_EQ(policy::EnterpriseInstallAttributes::LOCK_SUCCESS,
              connector_->LockDevice(username));
    loop_.RunAllPending();
  }

  void PrepareProfile(const std::string& username) {
    MockSessionManagerClient* session_managed_client =
        dbus_thread_manager_.mock_session_manager_client();
    EXPECT_CALL(*session_managed_client, StartSession(_));

    // crypto::Encryptor::SetCounter wants exactly 128 bits, and
    // ParallelAuthenticator CHECKs on that.
    CryptohomeBlob blob;
    for (size_t i = 0; i < 16; ++i)
      blob.push_back(i);
    EXPECT_CALL(*cryptohome_, GetSystemSalt())
        .WillRepeatedly(Return(blob));
    EXPECT_CALL(*cryptohome_, AsyncMount(_, _, _, _))
        .WillRepeatedly(Return(true));

    scoped_refptr<Authenticator> authenticator =
        LoginUtils::Get()->CreateAuthenticator(this);
    authenticator->CompleteLogin(ProfileManager::GetDefaultProfile(),
                                 username,
                                 "password");

    GaiaAuthConsumer::ClientLoginResult credentials;
    LoginUtils::Get()->PrepareProfile(
        username, "password", credentials, false, true, false, this);
    loop_.RunAllPending();
  }

  TestURLFetcher* PrepareOAuthFetcher(const std::string& expected_url) {
    TestURLFetcher* fetcher = test_url_fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    EXPECT_TRUE(fetcher->delegate());
    EXPECT_TRUE(StartsWithASCII(fetcher->GetOriginalURL().spec(),
                                expected_url,
                                true));
    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_response_code(200);
    fetcher->set_status(net::URLRequestStatus());
    return fetcher;
  }

  TestURLFetcher* PrepareDMServiceFetcher(
      const std::string& expected_url,
      const em::DeviceManagementResponse& response) {
    TestURLFetcher* fetcher = test_url_fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    EXPECT_TRUE(fetcher->delegate());
    EXPECT_TRUE(StartsWithASCII(fetcher->GetOriginalURL().spec(),
                                expected_url,
                                true));
    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_response_code(200);
    fetcher->set_status(net::URLRequestStatus());
    std::string data;
    EXPECT_TRUE(response.SerializeToString(&data));
    fetcher->SetResponseString(data);
    return fetcher;
  }

  TestURLFetcher* PrepareDMRegisterFetcher() {
    em::DeviceManagementResponse response;
    em::DeviceRegisterResponse* register_response =
        response.mutable_register_response();
    register_response->set_device_management_token(kDMToken);
    return PrepareDMServiceFetcher(kDMRegisterRequest, response);
  }

  TestURLFetcher* PrepareDMPolicyFetcher() {
    em::DeviceManagementResponse response;
    response.mutable_policy_response()->add_response();
    return PrepareDMServiceFetcher(kDMPolicyRequest, response);
  }

 protected:
  ScopedStubCrosEnabler stub_cros_enabler_;

  MessageLoop loop_;
  TestingBrowserProcess* browser_process_;
  ScopedTestingLocalState local_state_;

  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  IOThread io_thread_state_;

  MockDBusThreadManager dbus_thread_manager_;
  TestURLFetcherFactory test_url_fetcher_factory_;

  policy::BrowserPolicyConnector* connector_;
  MockCryptohomeLibrary* cryptohome_;
  Profile* prepared_profile_;

 private:
  ScopedTempDir scoped_temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsTestBase);
};

class LoginUtilsTest : public LoginUtilsTestBase<testing::Test> {
};

class LoginUtilsBlockingLoginTest
    : public LoginUtilsTestBase<testing::TestWithParam<int> > {
};

TEST_F(LoginUtilsTest, NormalLoginDoesntBlock) {
  UserManager* user_manager = UserManager::Get();
  EXPECT_FALSE(user_manager->user_is_logged_in());
  EXPECT_FALSE(connector_->IsEnterpriseManaged());
  EXPECT_FALSE(prepared_profile_);

  // The profile will be created without waiting for a policy response.
  PrepareProfile(kUsername);

  EXPECT_TRUE(prepared_profile_);
  EXPECT_TRUE(user_manager->user_is_logged_in());
  EXPECT_EQ(kUsername, user_manager->logged_in_user().email());
}

TEST_F(LoginUtilsTest, EnterpriseLoginDoesntBlockForNormalUser) {
  UserManager* user_manager = UserManager::Get();
  EXPECT_FALSE(user_manager->user_is_logged_in());
  EXPECT_FALSE(connector_->IsEnterpriseManaged());
  EXPECT_FALSE(prepared_profile_);

  // Enroll the device.
  LockDevice(kUsername);

  EXPECT_FALSE(user_manager->user_is_logged_in());
  EXPECT_TRUE(connector_->IsEnterpriseManaged());
  EXPECT_EQ(kDomain, connector_->GetEnterpriseDomain());
  EXPECT_FALSE(prepared_profile_);

  // Login with a non-enterprise user shouldn't block.
  PrepareProfile(kUsernameOtherDomain);

  EXPECT_TRUE(prepared_profile_);
  EXPECT_TRUE(user_manager->user_is_logged_in());
  EXPECT_EQ(kUsernameOtherDomain, user_manager->logged_in_user().email());
}

TEST_P(LoginUtilsBlockingLoginTest, EnterpriseLoginBlocksForEnterpriseUser) {
  UserManager* user_manager = UserManager::Get();
  EXPECT_FALSE(user_manager->user_is_logged_in());
  EXPECT_FALSE(connector_->IsEnterpriseManaged());
  EXPECT_FALSE(prepared_profile_);

  // Enroll the device.
  LockDevice(kUsername);

  EXPECT_FALSE(user_manager->user_is_logged_in());
  EXPECT_TRUE(connector_->IsEnterpriseManaged());
  EXPECT_EQ(kDomain, connector_->GetEnterpriseDomain());
  EXPECT_FALSE(prepared_profile_);

  // Login with a user of the enterprise domain waits for policy.
  PrepareProfile(kUsername);

  EXPECT_FALSE(prepared_profile_);
  EXPECT_TRUE(user_manager->user_is_logged_in());

  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  TestURLFetcher* fetcher;

  // |steps| is the test parameter, and is the number of successful fetches.
  // The first incomplete fetch will fail. In any case, the profile creation
  // should resume.
  int steps = GetParam();

  do {
    if (steps < 1) break;

    // Fake OAuth token retrieval:
    fetcher = PrepareOAuthFetcher(gaia_urls->get_oauth_token_url());
    net::ResponseCookies cookies;
    cookies.push_back(kOAuthTokenCookie);
    fetcher->set_cookies(cookies);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    if (steps < 2) break;

    // Fake OAuth access token retrieval:
    fetcher = PrepareOAuthFetcher(gaia_urls->oauth_get_access_token_url());
    fetcher->SetResponseString(kOAuthGetAccessTokenData);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    if (steps < 3) break;

    // Fake OAuth service token retrieval:
    fetcher = PrepareOAuthFetcher(gaia_urls->oauth_wrap_bridge_url());
    fetcher->SetResponseString(kOAuthServiceTokenData);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    // The cloud policy subsystem is now ready to fetch the dmtoken and the user
    // policy.
    loop_.RunAllPending();
    if (steps < 4) break;

    fetcher = PrepareDMRegisterFetcher();
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    // The policy fetch job has now been scheduled, run it:
    loop_.RunAllPending();
    if (steps < 5) break;

    // Verify that there is no profile prepared just before the policy fetch.
    EXPECT_FALSE(prepared_profile_);

    fetcher = PrepareDMPolicyFetcher();
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  } while (0);

  if (steps < 5) {
    // Verify that the profile hasn't been created yet.
    EXPECT_FALSE(prepared_profile_);

    // Make the current fetcher fail.
    TestURLFetcher* fetcher = test_url_fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    EXPECT_TRUE(fetcher->delegate());
    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_response_code(500);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  // The profile is finally ready:
  EXPECT_TRUE(prepared_profile_);
}

INSTANTIATE_TEST_CASE_P(
    LoginUtilsBlockingLoginTestInstance,
    LoginUtilsBlockingLoginTest,
    testing::Values(0, 1, 2, 3, 4, 5));

}  // namespace

}
