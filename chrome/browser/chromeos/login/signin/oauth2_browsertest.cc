// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using net::test_server::HungResponse;

namespace chromeos {

namespace {

// Email of owner account for test.
const char kTestGaiaId[] = "12345";
const char kTestEmail[] = "username@gmail.com";
const char kTestRawEmail[] = "User.Name@gmail.com";
const char kTestAccountPassword[] = "fake-password";
const char kTestAccountServices[] = "[]";
const char kTestAuthCode[] = "fake-auth-code";
const char kTestGaiaUberToken[] = "fake-uber-token";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestAuthSIDCookie[] = "fake-auth-SID-cookie";
const char kTestAuthLSIDCookie[] = "fake-auth-LSID-cookie";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";
const char kTestSession2SIDCookie[] = "fake-session2-SID-cookie";
const char kTestSession2LSIDCookie[] = "fake-session2-LSID-cookie";
const char kTestIdTokenAdvancedProtectionEnabled[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbInRpYSJdIH0="  // payload: { "services": ["tia"] }
    ".dummy-signature";
const char kTestIdTokenAdvancedProtectionDisabled[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbXSB9"  // payload: { "services": [] }
    ".dummy-signature";

CoreAccountId PickAccountId(Profile* profile,
                            const std::string& gaia_id,
                            const std::string& email) {
  return IdentityManagerFactory::GetInstance()
      ->GetForProfile(profile)
      ->PickAccountIdForAccount(gaia_id, email);
}

const char* BoolToString(bool value) {
  return value ? "true" : "false";
}

class OAuth2LoginManagerStateWaiter : public OAuth2LoginManager::Observer {
 public:
  explicit OAuth2LoginManagerStateWaiter(Profile* profile)
      : profile_(profile),
        waiting_for_state_(false),
        final_state_(OAuth2LoginManager::SESSION_RESTORE_NOT_STARTED) {}

  void WaitForStates(
      const std::set<OAuth2LoginManager::SessionRestoreState>& states) {
    DCHECK(!waiting_for_state_);
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile_);
    states_ = states;
    if (states_.find(login_manager->state()) != states_.end()) {
      final_state_ = login_manager->state();
      return;
    }

    waiting_for_state_ = true;
    login_manager->AddObserver(this);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    login_manager->RemoveObserver(this);
  }

  OAuth2LoginManager::SessionRestoreState final_state() { return final_state_; }

 private:
  // OAuth2LoginManager::Observer overrides.
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) override {
    if (!waiting_for_state_)
      return;

    if (states_.find(state) == states_.end())
      return;

    final_state_ = state;
    waiting_for_state_ = false;
    run_loop_->Quit();
  }

  Profile* profile_;
  std::set<OAuth2LoginManager::SessionRestoreState> states_;
  bool waiting_for_state_;
  OAuth2LoginManager::SessionRestoreState final_state_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2LoginManagerStateWaiter);
};

// Blocks a thread associated with a given |task_runner| on construction and
// unblocks it on destruction.
class ThreadBlocker {
 public:
  explicit ThreadBlocker(base::SingleThreadTaskRunner* task_runner)
      : unblock_event_(new base::WaitableEvent(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED)) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&BlockThreadOnThread, base::Owned(unblock_event_)));
  }
  ~ThreadBlocker() { unblock_event_->Signal(); }

 private:
  // Blocks the target thread until |event| is signaled.
  static void BlockThreadOnThread(base::WaitableEvent* event) { event->Wait(); }

  // |unblock_event_| is deleted after BlockThreadOnThread returns.
  base::WaitableEvent* const unblock_event_;

  DISALLOW_COPY_AND_ASSIGN(ThreadBlocker);
};

// Helper class that is added as a RequestMonitor of embedded test server to
// wait for a request to happen and defer it until Unblock is called.
class RequestDeferrer {
 public:
  RequestDeferrer()
      : blocking_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        start_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void UnblockRequest() { blocking_event_.Signal(); }

  void WaitForRequestToStart() {
    // If we have already served the request, bail out.
    if (start_event_.IsSignaled())
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void InterceptRequest(const HttpRequest& request) {
    start_event_.Signal();
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&RequestDeferrer::QuitRunnerOnUIThread,
                                  base::Unretained(this)));
    blocking_event_.Wait();
  }

 private:
  void QuitRunnerOnUIThread() {
    if (run_loop_)
      run_loop_->Quit();
  }

  base::WaitableEvent blocking_event_;
  base::WaitableEvent start_event_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RequestDeferrer);
};

}  // namespace

class OAuth2Test : public OobeBaseTest {
 protected:
  OAuth2Test() = default;
  ~OAuth2Test() override = default;

  // OobeBaseTest overrides.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);

    // Disable sync since we don't really need this for these tests and it also
    // makes OAuth2Test.MergeSession test flaky http://crbug.com/408867.
    command_line->AppendSwitch(switches::kDisableSync);
  }

  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestMonitor(
        base::Bind(&OAuth2Test::InterceptRequest, base::Unretained(this)));
  }

  void SetupGaiaServerForNewAccount(bool is_under_advanced_protection) {
    FakeGaia::MergeSessionParams params;
    params.auth_sid_cookie = kTestAuthSIDCookie;
    params.auth_lsid_cookie = kTestAuthLSIDCookie;
    params.auth_code = kTestAuthCode;
    params.refresh_token = kTestRefreshToken;
    params.access_token = kTestAuthLoginAccessToken;
    params.gaia_uber_token = kTestGaiaUberToken;
    params.session_sid_cookie = kTestSessionSIDCookie;
    params.session_lsid_cookie = kTestSessionLSIDCookie;
    params.id_token = is_under_advanced_protection
                          ? kTestIdTokenAdvancedProtectionEnabled
                          : kTestIdTokenAdvancedProtectionDisabled;
    fake_gaia_.fake_gaia()->SetMergeSessionParams(params);
    fake_gaia_.SetupFakeGaiaForLogin(kTestEmail, kTestGaiaId,
                                     kTestRefreshToken);
  }

  const extensions::Extension* LoadMergeSessionExtension() {
    extensions::ChromeTestExtensionLoader loader(GetProfile());
    scoped_refptr<const extensions::Extension> extension =
        loader.LoadExtension(test_data_dir_.AppendASCII("extensions")
                                 .AppendASCII("api_test")
                                 .AppendASCII("merge_session"));
    return extension.get();
  }

  void SetupGaiaServerForUnexpiredAccount() {
    FakeGaia::MergeSessionParams params;
    params.email = kTestEmail;
    fake_gaia_.fake_gaia()->SetMergeSessionParams(params);
    fake_gaia_.SetupFakeGaiaForLogin(kTestEmail, kTestGaiaId,
                                     kTestRefreshToken);
  }

  void SetupGaiaServerForExpiredAccount() {
    FakeGaia::MergeSessionParams params;
    params.gaia_uber_token = kTestGaiaUberToken;
    params.session_sid_cookie = kTestSession2SIDCookie;
    params.session_lsid_cookie = kTestSession2LSIDCookie;
    fake_gaia_.fake_gaia()->SetMergeSessionParams(params);
    fake_gaia_.SetupFakeGaiaForLogin(kTestEmail, kTestGaiaId,
                                     kTestRefreshToken);
  }

  void LoginAsExistingUser() {
    // PickAccountId does not work at this point as the primary user profile has
    // not yet been created.
    const std::string email = kTestEmail;
    EXPECT_EQ(GetOAuthStatusFromLocalState(email),
              user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

    // Try login.  Primary profile has changed.
    ash::LoginScreenTestApi::SubmitPassword(
        AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId),
        kTestAccountPassword, true /*check_if_submittable */);
    test::WaitForPrimaryUserSessionStart();
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    CoreAccountId account_id = PickAccountId(profile, kTestGaiaId, kTestEmail);
    ASSERT_EQ(email, account_id.ToString());

    // Wait for the session merge to finish.
    WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

    // Check for existence of refresh token.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id));

    EXPECT_EQ(GetOAuthStatusFromLocalState(account_id.ToString()),
              user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  }

  bool TryToLogin(const AccountId& account_id, const std::string& password) {
    if (!AddUserToSession(account_id, password))
      return false;

    if (const user_manager::User* active_user =
            user_manager::UserManager::Get()->GetActiveUser()) {
      return active_user->GetAccountId() == account_id;
    }

    return false;
  }

  user_manager::User::OAuthTokenStatus GetOAuthStatusFromLocalState(
      const std::string& account_id) const {
    PrefService* local_state = g_browser_process->local_state();
    const base::DictionaryValue* prefs_oauth_status =
        local_state->GetDictionary("OAuthTokenStatus");
    int oauth_token_status = user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN;
    if (prefs_oauth_status &&
        prefs_oauth_status->GetIntegerWithoutPathExpansion(
            account_id, &oauth_token_status)) {
      user_manager::User::OAuthTokenStatus result =
          static_cast<user_manager::User::OAuthTokenStatus>(oauth_token_status);
      return result;
    }
    return user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN;
  }

 protected:
  // OobeBaseTest overrides.
  Profile* GetProfile() {
    if (user_manager::UserManager::Get()->GetActiveUser())
      return ProfileManager::GetPrimaryUserProfile();

    return ProfileManager::GetActiveUserProfile();
  }

  bool AddUserToSession(const AccountId& account_id,
                        const std::string& password) {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (!controller) {
      ADD_FAILURE();
      return false;
    }

    UserContext user_context(user_manager::UserType::USER_TYPE_REGULAR,
                             account_id);
    user_context.SetKey(Key(password));
    controller->Login(user_context, SigninSpecifics());
    test::WaitForPrimaryUserSessionStart();
    const user_manager::UserList& logged_users =
        user_manager::UserManager::Get()->GetLoggedInUsers();
    for (user_manager::UserList::const_iterator it = logged_users.begin();
         it != logged_users.end(); ++it) {
      if ((*it)->GetAccountId() == account_id)
        return true;
    }
    return false;
  }

  void CheckSessionState(OAuth2LoginManager::SessionRestoreState state) {
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(GetProfile());
    ASSERT_EQ(state, login_manager->state());
  }

  void SetSessionRestoreState(OAuth2LoginManager::SessionRestoreState state) {
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(GetProfile());
    login_manager->SetSessionRestoreState(state);
  }

  void WaitForMergeSessionCompletion(
      OAuth2LoginManager::SessionRestoreState final_state) {
    // Wait for the session merge to finish.
    std::set<OAuth2LoginManager::SessionRestoreState> states;
    states.insert(OAuth2LoginManager::SESSION_RESTORE_DONE);
    states.insert(OAuth2LoginManager::SESSION_RESTORE_FAILED);
    states.insert(OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED);
    OAuth2LoginManagerStateWaiter merge_session_waiter(GetProfile());
    merge_session_waiter.WaitForStates(states);
    EXPECT_EQ(merge_session_waiter.final_state(), final_state);
  }

  void StartNewUserSession(bool wait_for_merge,
                           bool is_under_advanced_protection) {
    SetupGaiaServerForNewAccount(is_under_advanced_protection);
    SimulateNetworkOnline();
    WaitForGaiaPageLoad();

    // Use capitalized and dotted user name on purpose to make sure
    // our email normalization kicks in.
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(kTestRawEmail, kTestAccountPassword,
                                  kTestAccountServices);
    test::WaitForPrimaryUserSessionStart();

    if (wait_for_merge) {
      // Wait for the session merge to finish.
      WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);
    }
  }

  OAuth2LoginManager::SessionRestoreStrategy GetSessionRestoreStrategy() {
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(GetProfile());
    return login_manager->restore_strategy_;
  }

  void InterceptRequest(const HttpRequest& request) {
    const GURL request_url =
        GURL("http://localhost").Resolve(request.relative_url);
    auto it = request_deferers_.find(request_url.path());
    if (it == request_deferers_.end())
      return;

    it->second->InterceptRequest(request);
  }

  void AddRequestDeferer(const std::string& path,
                         RequestDeferrer* request_deferer) {
    DCHECK(request_deferers_.find(path) == request_deferers_.end());
    request_deferers_[path] = request_deferer;
  }

  void SimulateNetworkOnline() {
    network_portal_detector_.SimulateDefaultNetworkState(
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  }

  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

 private:
  base::FilePath test_data_dir_;
  std::map<std::string, RequestDeferrer*> request_deferers_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2Test);
};

class CookieReader {
 public:
  CookieReader() = default;
  ~CookieReader() = default;

  void ReadCookies(Profile* profile) {
    base::RunLoop run_loop;
    content::BrowserContext::GetDefaultStoragePartition(profile)
        ->GetCookieManagerForBrowserProcess()
        ->GetAllCookies(base::BindOnce(&CookieReader::OnGotAllCookies,
                                       base::Unretained(this),
                                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  std::string GetCookieValue(const std::string& name) {
    for (const auto& item : cookie_list_) {
      if (item.Name() == name) {
        return item.Value();
      }
    }
    return std::string();
  }

 private:
  void OnGotAllCookies(base::OnceClosure callback,
                       const net::CookieList& cookies) {
    cookie_list_ = cookies;
    std::move(callback).Run();
  }

  net::CookieList cookie_list_;

  DISALLOW_COPY_AND_ASSIGN(CookieReader);
};

// PRE_MergeSession is testing merge session for a new profile.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_PRE_PRE_MergeSession) {
  StartNewUserSession(/*wait_for_merge=*/true,
                      /*is_under_advanced_protection=*/false);
  // Check for existence of refresh token.
  CoreAccountId account_id =
      PickAccountId(GetProfile(), kTestGaiaId, kTestEmail);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id));

  EXPECT_EQ(GetOAuthStatusFromLocalState(account_id.ToString()),
            user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  CookieReader cookie_reader;
  cookie_reader.ReadCookies(GetProfile());
  EXPECT_EQ(cookie_reader.GetCookieValue("SID"), kTestSessionSIDCookie);
  EXPECT_EQ(cookie_reader.GetCookieValue("LSID"), kTestSessionLSIDCookie);
}

// MergeSession test is running merge session process for an existing profile
// that was generated in PRE_PRE_PRE_MergeSession test. In this test, we
// are not running /MergeSession process since the /ListAccounts call confirms
// that the session is not stale.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_PRE_MergeSession) {
  SetupGaiaServerForUnexpiredAccount();
  SimulateNetworkOnline();
  LoginAsExistingUser();
  CookieReader cookie_reader;
  cookie_reader.ReadCookies(GetProfile());
  // These are still cookie values from the initial session since
  // /ListAccounts
  EXPECT_EQ(cookie_reader.GetCookieValue("SID"), kTestSessionSIDCookie);
  EXPECT_EQ(cookie_reader.GetCookieValue("LSID"), kTestSessionLSIDCookie);
}

// MergeSession test is running merge session process for an existing profile
// that was generated in PRE_PRE_MergeSession test.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_MergeSession) {
  SetupGaiaServerForExpiredAccount();
  SimulateNetworkOnline();
  LoginAsExistingUser();
  CookieReader cookie_reader;
  cookie_reader.ReadCookies(GetProfile());
  // These should be cookie values that we generated by calling /MergeSession,
  // since /ListAccounts should have tell us that the initial session cookies
  // are stale.
  EXPECT_EQ(cookie_reader.GetCookieValue("SID"), kTestSession2SIDCookie);
  EXPECT_EQ(cookie_reader.GetCookieValue("LSID"), kTestSession2LSIDCookie);
}

// MergeSession test is attempting to merge session for an existing profile
// that was generated in PRE_PRE_MergeSession test. This attempt should fail
// since FakeGaia instance isn't configured to return relevant tokens/cookies.
IN_PROC_BROWSER_TEST_F(OAuth2Test, MergeSession) {
  SimulateNetworkOnline();

  EXPECT_EQ(1, ash::LoginScreenTestApi::GetUsersCount());

  // PickAccountId does not work at this point as the primary user profile has
  // not yet been created.
  const std::string account_id = kTestEmail;
  EXPECT_EQ(GetOAuthStatusFromLocalState(account_id),
            user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

  EXPECT_TRUE(
      TryToLogin(AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId),
                 kTestAccountPassword));

  ASSERT_EQ(account_id,
            PickAccountId(GetProfile(), kTestGaiaId, kTestEmail).ToString());

  // Wait for the session merge to finish.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_FAILED);

  EXPECT_EQ(GetOAuthStatusFromLocalState(account_id),
            user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
}

// Sets up a new user with stored refresh token.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_OverlappingContinueSessionRestore) {
  StartNewUserSession(/*wait_for_merge=*/true,
                      /*is_under_advanced_protection=*/false);
}

// Tests that ContinueSessionRestore could be called multiple times.
IN_PROC_BROWSER_TEST_F(OAuth2Test, DISABLED_OverlappingContinueSessionRestore) {
  SetupGaiaServerForUnexpiredAccount();
  SimulateNetworkOnline();

  // Blocks database thread to control TokenService::LoadCredentials timing.
  // TODO(achuith): Fix this. crbug.com/753615.
  auto thread_blocker = std::make_unique<ThreadBlocker>(nullptr);

  // Signs in as the existing user created in pre test.
  EXPECT_TRUE(
      TryToLogin(AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId),
                 kTestAccountPassword));

  // Session restore should be using the saved tokens.
  EXPECT_EQ(OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN,
            GetSessionRestoreStrategy());

  // Checks that refresh token is not yet loaded.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  const CoreAccountId account_id =
      PickAccountId(GetProfile(), kTestGaiaId, kTestEmail);
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(account_id));

  // Invokes ContinueSessionRestore multiple times and there should be
  // no DCHECK failures.
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(GetProfile());
  login_manager->ContinueSessionRestore();
  login_manager->ContinueSessionRestore();

  // Let go DB thread to finish TokenService::LoadCredentials.
  thread_blocker.reset();

  // Session restore can finish normally and token is loaded.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id));
}

// Tests that user session is terminated if merge session fails for an online
// sign-in. This is necessary to prevent policy exploit.
// See http://crbug.com/677312
IN_PROC_BROWSER_TEST_F(OAuth2Test, TerminateOnBadMergeSessionAfterOnlineAuth) {
  SimulateNetworkOnline();
  WaitForGaiaPageLoad();

  content::WindowedNotificationObserver termination_waiter(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  // Configure FakeGaia so that online auth succeeds but merge session fails.
  FakeGaia::MergeSessionParams params;
  params.auth_sid_cookie = kTestAuthSIDCookie;
  params.auth_lsid_cookie = kTestAuthLSIDCookie;
  params.auth_code = kTestAuthCode;
  params.refresh_token = kTestRefreshToken;
  params.access_token = kTestAuthLoginAccessToken;
  fake_gaia_.fake_gaia()->SetMergeSessionParams(params);

  // Simulate an online sign-in.
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(kTestRawEmail, kTestAccountPassword,
                                kTestAccountServices);

  // User session should be terminated.
  termination_waiter.Wait();

  // Merge session should fail. Check after |termination_waiter| to ensure
  // user profile is initialized and there is an OAuth2LoginManage.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_FAILED);
}

IN_PROC_BROWSER_TEST_F(OAuth2Test, VerifyInAdvancedProtectionAfterOnlineAuth) {
  StartNewUserSession(/*wait_for_merge=*/true,
                      /*is_under_advanced_protection=*/true);

  // Verify that AccountInfo is properly updated.
  auto* identity_manager =
      IdentityManagerFactory::GetInstance()->GetForProfile(GetProfile());
  EXPECT_TRUE(
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              kTestEmail)
          ->is_under_advanced_protection);
}

IN_PROC_BROWSER_TEST_F(OAuth2Test,
                       VerifyNotInAdvancedProtectionAfterOnlineAuth) {
  StartNewUserSession(/*wait_for_merge=*/true,
                      /*is_under_advanced_protection=*/false);

  // Verify that AccountInfo is properly updated.
  auto* identity_manager =
      IdentityManagerFactory::GetInstance()->GetForProfile(GetProfile());
  EXPECT_FALSE(
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              kTestEmail)
          ->is_under_advanced_protection);
}

// Sets up a new user with stored refresh token.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_SetInvalidTokenStatus) {
  StartNewUserSession(/*wait_for_merge=*/true,
                      /*is_under_advanced_protection=*/false);
}

// Tests that an auth error marks invalid auth token status despite
// OAuth2LoginManager thinks merge session is done successfully
IN_PROC_BROWSER_TEST_F(OAuth2Test, SetInvalidTokenStatus) {
  RequestDeferrer list_accounts_request_deferer;
  AddRequestDeferer("/ListAccounts", &list_accounts_request_deferer);

  SetupGaiaServerForUnexpiredAccount();
  SimulateNetworkOnline();

  // Signs in as the existing user created in pre test.
  ExistingUserController* const controller =
      ExistingUserController::current_controller();
  UserContext user_context(
      user_manager::USER_TYPE_REGULAR,
      AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId));
  user_context.SetKey(Key(kTestAccountPassword));
  controller->Login(user_context, SigninSpecifics());

  // Wait until /ListAccounts request happens so that an auth error can be
  // generated after user profile is available but before merge session
  // finishes.
  list_accounts_request_deferer.WaitForRequestToStart();

  // Make sure that merge session is not finished.
  OAuth2LoginManager* const login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(GetProfile());
  ASSERT_NE(OAuth2LoginManager::SESSION_RESTORE_DONE, login_manager->state());

  // Generate an auth error.
  signin::SetInvalidRefreshTokenForAccount(
      IdentityManagerFactory::GetInstance()->GetForProfile(GetProfile()),
      PickAccountId(GetProfile(), kTestGaiaId, kTestEmail));

  // Let go /ListAccounts request.
  list_accounts_request_deferer.UnblockRequest();

  // Wait for the session merge to finish with success.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  // User oauth2 token status should be marked as invalid because of auth error
  // and regardless of the merge session outcome.
  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestEmail),
            user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
}

constexpr char kGooglePageContent[] =
    "<html><title>Hello!</title><script>alert('hello');</script>"
    "<body>Hello Google!</body></html>";
constexpr char kRandomPageContent[] =
    "<html><title>SomthingElse</title><body>I am SomethingElse</body></html>";
constexpr char kHelloPagePath[] = "/hello_google";
constexpr char kRandomPagePath[] = "/non_google_page";
constexpr char kMergeSessionPath[] = "/MergeSession";

// FakeGoogle serves content of http://www.google.com/hello_google page for
// merge session tests.
class FakeGoogle {
 public:
  FakeGoogle()
      : start_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
        merge_session_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  ~FakeGoogle() {}

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // The scheme and host of the URL is actually not important but required to
    // get a valid GURL in order to parse |request.relative_url|.
    GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
    std::string request_path = request_url.path();
    std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
    if (request_path == kHelloPagePath) {  // Serving "google" page.
      start_event_.Signal();
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     base::BindOnce(&FakeGoogle::QuitRunnerOnUIThread,
                                    base::Unretained(this)));

      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("text/html");
      http_response->set_content(kGooglePageContent);
    } else if (request_path == kRandomPagePath) {  // Serving "non-google" page.
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("text/html");
      http_response->set_content(kRandomPageContent);
    } else if (hang_merge_session_ && request_path == kMergeSessionPath) {
      merge_session_event_.Signal();
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     base::BindOnce(&FakeGoogle::QuitMergeRunnerOnUIThread,
                                    base::Unretained(this)));
      return std::make_unique<HungResponse>();
    } else {
      return std::unique_ptr<HttpResponse>();  // Request not understood.
    }

    return std::move(http_response);
  }

  // True if we have already served the test page.
  bool IsPageRequested() { return start_event_.IsSignaled(); }

  // Waits until we receive a request to serve the test page.
  void WaitForPageRequest() {
    // If we have already served the request, bail out.
    if (start_event_.IsSignaled())
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // Waits until we receive a request to serve the /MergeSession page.
  void WaitForMergeSessionPageRequest() {
    // If we have already served the request, bail out.
    if (merge_session_event_.IsSignaled())
      return;

    merge_session_run_loop_ = std::make_unique<base::RunLoop>();
    merge_session_run_loop_->Run();
  }

  void set_hang_merge_session() { hang_merge_session_ = true; }

 private:
  void QuitRunnerOnUIThread() {
    if (run_loop_)
      run_loop_->Quit();
  }
  void QuitMergeRunnerOnUIThread() {
    if (merge_session_run_loop_)
      merge_session_run_loop_->Quit();
  }
  // This event will tell us when we actually see HTTP request on the server
  // side. It should be signalled only after the page/XHR throttle had been
  // removed (after merge session completes).
  base::WaitableEvent start_event_;
  base::WaitableEvent merge_session_event_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<base::RunLoop> merge_session_run_loop_;
  bool hang_merge_session_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeGoogle);
};

class MergeSessionTest : public OAuth2Test,
                         public testing::WithParamInterface<bool> {
 protected:
  MergeSessionTest() = default;

  // OAuth2Test overrides.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OAuth2Test::SetUpCommandLine(command_line);

    // Get fake URL for fake google.com.
    const GURL& server_url = embedded_test_server()->base_url();
    GURL::Replacements replace_google_host;
    replace_google_host.SetHostStr("www.google.com");
    GURL google_url = server_url.ReplaceComponents(replace_google_host);
    fake_google_page_url_ = google_url.Resolve(kHelloPagePath);

    GURL::Replacements replace_non_google_host;
    replace_non_google_host.SetHostStr("www.somethingelse.org");
    GURL non_google_url = server_url.ReplaceComponents(replace_non_google_host);
    non_google_page_url_ = non_google_url.Resolve(kRandomPagePath);
  }

  void RegisterAdditionalRequestHandlers() override {
    OAuth2Test::RegisterAdditionalRequestHandlers();
    AddRequestDeferer("/MergeSession", &merge_session_deferer_);

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &FakeGoogle::HandleRequest, base::Unretained(&fake_google_)));
  }

 protected:
  void UnblockMergeSession() { merge_session_deferer_.UnblockRequest(); }

  virtual void WaitForMergeSessionToStart() {
    merge_session_deferer_.WaitForRequestToStart();
  }

  bool do_async_xhr() const { return GetParam(); }

  void JsExpectAsync(content::WebContents* web_contents,
                     const std::string& expression) {
    content::DOMMessageQueue dom_message_queue(web_contents);
    content::ExecuteScriptAsync(
        web_contents,
        "window.domAutomationController.send(!!(" + expression + "));");
  }

  void JsExpectOnBackgroundPageAsync(const std::string& extension_id,
                                     const std::string& expression) {
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(GetProfile());
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    if (host == NULL) {
      ADD_FAILURE() << "Extension " << extension_id
                    << " has no background page.";
      return;
    }

    JsExpectAsync(host->host_contents(), expression);
  }

  void JsExpect(content::WebContents* contents, const std::string& expression) {
    bool result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "window.domAutomationController.send(!!(" + expression + "));",
        &result));
    ASSERT_TRUE(result) << expression;
  }

  const GURL& GetBackGroundPageUrl(const std::string& extension_id) {
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(GetProfile());
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    return host->host_contents()->GetURL();
  }

  void JsExpectOnBackgroundPage(const std::string& extension_id,
                                const std::string& expression) {
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(GetProfile());
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    if (host == NULL) {
      ADD_FAILURE() << "Extension " << extension_id
                    << " has no background page.";
      return;
    }

    JsExpect(host->host_contents(), expression);
  }

  FakeGoogle fake_google_;
  RequestDeferrer merge_session_deferer_;
  GURL fake_google_page_url_;
  GURL non_google_page_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MergeSessionTest);
};

Browser* FindOrCreateVisibleBrowser(Profile* profile) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  Browser* browser = displayer.browser();
  if (browser->tab_strip_model()->count() == 0)
    chrome::AddTabAt(browser, GURL(), -1, true);
  return browser;
}

IN_PROC_BROWSER_TEST_P(MergeSessionTest, PageThrottle) {
  StartNewUserSession(/*wait_for_merge=*/false,
                      /*is_under_advanced_protection=*/false);

  // Try to open a page from google.com.
  Browser* browser = FindOrCreateVisibleBrowser(GetProfile());
  ui_test_utils::NavigateToURLWithDisposition(
      browser, fake_google_page_url_, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // JavaScript dialog wait setup.
  content::WebContents* tab =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(tab);
  base::RunLoop dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.QuitClosure());

  // Wait until we get send merge session request.
  WaitForMergeSessionToStart();

  // Make sure the page is blocked by the throttle.
  EXPECT_FALSE(fake_google_.IsPageRequested());

  // Check that throttle page is displayed instead.
  base::string16 title;
  ui_test_utils::GetCurrentTabTitle(browser, &title);
  DVLOG(1) << "Loaded page at the start : " << title;

  // Unblock GAIA request.
  UnblockMergeSession();

  // Wait for the session merge to finish.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Make sure the test page is served.
  fake_google_.WaitForPageRequest();

  // Check that real page is no longer blocked by the throttle and that the
  // real page pops up JS dialog.
  dialog_wait.Run();
  js_dialog_manager->HandleJavaScriptDialog(tab, true, nullptr);

  ui_test_utils::GetCurrentTabTitle(browser, &title);
  DVLOG(1) << "Loaded page at the end : " << title;
}

IN_PROC_BROWSER_TEST_P(MergeSessionTest, Throttle) {
  StartNewUserSession(/*wait_for_merge=*/false,
                      /*is_under_advanced_protection=*/false);

  // Wait until we get send merge session request.
  WaitForMergeSessionToStart();

  // Run background page tests. The tests will just wait for XHR request
  // to complete.
  extensions::ResultCatcher catcher;

  std::unique_ptr<ExtensionTestMessageListener> non_google_xhr_listener(
      new ExtensionTestMessageListener("non-google-xhr-received", false));

  // Load extension with a background page. The background page will
  // attempt to load |fake_google_page_url_| via XHR.
  const extensions::Extension* ext = LoadMergeSessionExtension();
  ASSERT_TRUE(ext);

  // Kick off XHR request from the extension.
  JsExpectOnBackgroundPageAsync(
      ext->id(), base::StringPrintf("startThrottledTests('%s', '%s', %s, %s)",
                                    fake_google_page_url_.spec().c_str(),
                                    non_google_page_url_.spec().c_str(),
                                    BoolToString(do_async_xhr()),
                                    BoolToString(/*should_throttle=*/true)));
  ExtensionTestMessageListener listener("Both XHR's Opened", false);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Verify that we've sent XHR request from the extension side (async)...
  // The XMLHttpRequest.send() call is blocked when running synchronously
  // so cannot eval JavaScript.
  if (do_async_xhr()) {
    JsExpectOnBackgroundPage(ext->id(),
                             "googleRequestSent && !googleResponseReceived");
  }

  // ...but didn't see it on the server side yet.
  EXPECT_FALSE(fake_google_.IsPageRequested());

  // Unblock GAIA request.
  UnblockMergeSession();

  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Verify that we've sent XHR request from the extension side...
  // Wait until non-google XHR content to load first.
  ASSERT_TRUE(non_google_xhr_listener->WaitUntilSatisfied());

  if (!catcher.GetNextResult()) {
    std::string message = catcher.message();
    ADD_FAILURE() << "Tests failed: " << message;
  }

  EXPECT_TRUE(fake_google_.IsPageRequested());
}

// The test is too slow for the MSan configuration.
#if defined(MEMORY_SANITIZER)
#define MAYBE_XHRNotThrottled DISABLED_XHRNotThrottled
#else
#define MAYBE_XHRNotThrottled XHRNotThrottled
#endif
IN_PROC_BROWSER_TEST_P(MergeSessionTest, MAYBE_XHRNotThrottled) {
  StartNewUserSession(/*wait_for_merge=*/false,
                      /*is_under_advanced_protection=*/false);

  // Wait until we get send merge session request.
  WaitForMergeSessionToStart();

  // Unblock GAIA request.
  UnblockMergeSession();

  // Wait for the session merge to finish.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Run background page tests. The tests will just wait for XHR request
  // to complete.
  extensions::ResultCatcher catcher;

  std::unique_ptr<ExtensionTestMessageListener> non_google_xhr_listener(
      new ExtensionTestMessageListener("non-google-xhr-received", false));

  // Load extension with a background page. The background page will
  // attempt to load |fake_google_page_url_| via XHR.
  const extensions::Extension* ext = LoadMergeSessionExtension();
  ASSERT_TRUE(ext);

  // Kick off XHR request from the extension.
  JsExpectOnBackgroundPage(
      ext->id(),
      base::StringPrintf("startThrottledTests('%s', '%s', %s, %s)",
                         fake_google_page_url_.spec().c_str(),
                         non_google_page_url_.spec().c_str(),
                         BoolToString(do_async_xhr()), BoolToString(false)));

  if (do_async_xhr()) {
    // Verify that we've sent XHR request from the extension side...
    JsExpectOnBackgroundPage(ext->id(), "googleRequestSent");

    // Wait until non-google XHR content to load.
    ASSERT_TRUE(non_google_xhr_listener->WaitUntilSatisfied());
  } else {
    content::RunAllTasksUntilIdle();
  }

  if (!catcher.GetNextResult()) {
    std::string message = catcher.message();
    ADD_FAILURE() << "Tests failed: " << message;
  }

  if (do_async_xhr()) {
    EXPECT_TRUE(fake_google_.IsPageRequested());
  }
}

class MergeSessionTimeoutTest : public MergeSessionTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MergeSessionTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kShortMergeSessionTimeoutForTest);
  }

  void RegisterAdditionalRequestHandlers() override {
    OAuth2Test::RegisterAdditionalRequestHandlers();

    // Do not defer /MergeSession requests (like the base class does) because
    // this test will intentionally hang that request to force a timeout.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &FakeGoogle::HandleRequest, base::Unretained(&fake_google_)));
  }

  void WaitForMergeSessionToStart() override {
    fake_google_.WaitForMergeSessionPageRequest();
  }
};

IN_PROC_BROWSER_TEST_P(MergeSessionTimeoutTest, XHRMergeTimeout) {
  fake_google_.set_hang_merge_session();

  StartNewUserSession(/*wait_for_merge=*/false,
                      /*is_under_advanced_protection=*/false);

  WaitForMergeSessionToStart();

  // Run background page tests. The tests will just wait for XHR request
  // to complete.
  extensions::ResultCatcher catcher;

  std::unique_ptr<ExtensionTestMessageListener> non_google_xhr_listener(
      new ExtensionTestMessageListener("non-google-xhr-received", false));

  // Load extension with a background page. The background page will
  // attempt to load |fake_google_page_url_| via XHR.
  const extensions::Extension* ext = LoadMergeSessionExtension();
  ASSERT_TRUE(ext);

  const base::Time start_time = base::Time::Now();

  // Kick off XHR request from the extension.
  JsExpectOnBackgroundPageAsync(
      ext->id(),
      base::StringPrintf("startThrottledTests('%s', '%s', %s, %s)",
                         fake_google_page_url_.spec().c_str(),
                         non_google_page_url_.spec().c_str(),
                         BoolToString(do_async_xhr()), BoolToString(true)));

  if (do_async_xhr()) {
    // Verify that we've sent XHR request from the extension side...
    JsExpectOnBackgroundPage(ext->id(),
                             "googleRequestSent && !googleResponseReceived");

    // ...but didn't see it on the server side yet.
    EXPECT_FALSE(fake_google_.IsPageRequested());

    // Wait until the last XHR load completes.
    ASSERT_TRUE(non_google_xhr_listener->WaitUntilSatisfied());

    // If the test runs in less than the test timeout (1 second) then we know
    // that there was no delay. However a slowly running test can still take
    // longer than the timeout.
    base::TimeDelta test_duration = base::Time::Now() - start_time;
    EXPECT_GE(test_duration, base::TimeDelta::FromSeconds(1));
  } else {
    content::RunAllTasksUntilIdle();
  }

  if (!catcher.GetNextResult()) {
    std::string message = catcher.message();
    ADD_FAILURE() << "Tests failed: " << message;
  }

  if (do_async_xhr()) {
    EXPECT_TRUE(fake_google_.IsPageRequested());
  }

  // Because this test has hung the /MergeSession response the
  // UserSessionManager is still observing the OAuth2LoginManager which fails
  // a DCHECK in ~OAuth2LoginManager. Manually change the state to avoid this.
  SetSessionRestoreState(
      OAuth2LoginManager::SessionRestoreState::SESSION_RESTORE_FAILED);
}

INSTANTIATE_TEST_SUITE_P(All, MergeSessionTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(All, MergeSessionTimeoutTest, testing::Bool());

}  // namespace chromeos
