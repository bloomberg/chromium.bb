// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

using guest_view::GuestViewManager;
using login_ui_test_utils::ExecuteJsToSigninInSigninFrame;
using login_ui_test_utils::WaitUntilUIReady;

namespace {

struct ContentInfo {
  ContentInfo(content::WebContents* contents,
              int pid,
              content::StoragePartition* storage_partition) {
    this->contents = contents;
    this->pid = pid;
    this->storage_partition = storage_partition;
  }

  content::WebContents* contents;
  int pid;
  content::StoragePartition* storage_partition;
};

ContentInfo NavigateAndGetInfo(
    Browser* browser,
    const GURL& url,
    WindowOpenDisposition disposition) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, disposition,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  content::RenderProcessHost* process = contents->GetRenderProcessHost();
  return ContentInfo(contents, process->GetID(),
                     process->GetStoragePartition());
}

// Returns a new WebUI object for the WebContents from |arg0|.
ACTION(ReturnNewWebUI) {
  return new content::WebUIController(arg0);
}

GURL GetSigninPromoURL() {
  return signin::GetPromoURL(
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT, false);
}

// Mock the TestChromeWebUIControllerFactory::WebUIProvider to prove that we are
// not called as expected.
class FooWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  MOCK_METHOD2(NewWebUI, content::WebUIController*(content::WebUI* web_ui,
                                                   const GURL& url));
};

class MockLoginUIObserver : public LoginUIService::Observer {
 public:
  MOCK_METHOD0(OnUntrustedLoginUIShown, void());
};

const char kFooWebUIURL[] = "chrome://foo/";

bool AddToSet(std::set<content::WebContents*>* set,
              content::WebContents* web_contents) {
  set->insert(web_contents);
  return false;
}

// This class is used to mock out virtual methods with side effects so that
// tests below can ensure they are called without causing side effects.
class MockInlineSigninHelper : public InlineSigninHelper {
 public:
  MockInlineSigninHelper(
      base::WeakPtr<InlineLoginHandlerImpl> handler,
      net::URLRequestContextGetter* getter,
      Profile* profile,
      const GURL& current_url,
      const std::string& email,
      const std::string& gaia_id,
      const std::string& password,
      const std::string& session_index,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id,
      bool choose_what_to_sync,
      bool confirm_untrusted_signin);

  MOCK_METHOD1(OnClientOAuthSuccess, void(const ClientOAuthResult& result));
  MOCK_METHOD1(OnClientOAuthFailure, void(const GoogleServiceAuthError& error));
  MOCK_METHOD7(CreateSyncStarter,
               void(Browser*,
                    content::WebContents*,
                    const GURL&,
                    const GURL&,
                    const std::string&,
                    OneClickSigninSyncStarter::StartSyncMode,
                    OneClickSigninSyncStarter::ConfirmationRequired));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInlineSigninHelper);
};

MockInlineSigninHelper::MockInlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    net::URLRequestContextGetter* getter,
    Profile* profile,
    const GURL& current_url,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& session_index,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id,
    bool choose_what_to_sync,
    bool confirm_untrusted_signin)
  : InlineSigninHelper(handler,
                       getter,
                       profile,
                       current_url,
                       email,
                       gaia_id,
                       password,
                       session_index,
                       auth_code,
                       signin_scoped_device_id,
                       choose_what_to_sync,
                       confirm_untrusted_signin) {}

// This class is used to mock out virtual methods with side effects so that
// tests below can ensure they are called without causing side effects.
class MockSyncStarterInlineSigninHelper : public InlineSigninHelper {
 public:
  MockSyncStarterInlineSigninHelper(
      base::WeakPtr<InlineLoginHandlerImpl> handler,
      net::URLRequestContextGetter* getter,
      Profile* profile,
      const GURL& current_url,
      const std::string& email,
      const std::string& gaia_id,
      const std::string& password,
      const std::string& session_index,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id,
      bool choose_what_to_sync,
      bool confirm_untrusted_signin);

  MOCK_METHOD7(CreateSyncStarter,
               void(Browser*,
                    content::WebContents*,
                    const GURL&,
                    const GURL&,
                    const std::string&,
                    OneClickSigninSyncStarter::StartSyncMode,
                    OneClickSigninSyncStarter::ConfirmationRequired));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSyncStarterInlineSigninHelper);
};

MockSyncStarterInlineSigninHelper::MockSyncStarterInlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    net::URLRequestContextGetter* getter,
    Profile* profile,
    const GURL& current_url,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& session_index,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id,
    bool choose_what_to_sync,
    bool confirm_untrusted_signin)
  : InlineSigninHelper(handler,
                       getter,
                       profile,
                       current_url,
                       email,
                       gaia_id,
                       password,
                       session_index,
                       auth_code,
                       signin_scoped_device_id,
                       choose_what_to_sync,
                       confirm_untrusted_signin) {}

}  // namespace

class InlineLoginUIBrowserTest : public InProcessBrowserTest {
 public:
  InlineLoginUIBrowserTest() {}

  void SetUpSigninManager(const std::string& username);
  void EnableSigninAllowed(bool enable);
  void EnableOneClick(bool enable);
  void AddEmailToOneClickRejectedList(const std::string& email);
  void AllowSigninCookies(bool enable);
  void SetAllowedUsernamePattern(const std::string& pattern);

 protected:
  content::WebContents* web_contents() { return nullptr; }
};

void InlineLoginUIBrowserTest::SetUpSigninManager(const std::string& username) {
  if (username.empty())
    return;

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(browser()->profile());
  signin_manager->SetAuthenticatedAccountInfo(username, username);
}

void InlineLoginUIBrowserTest::EnableSigninAllowed(bool enable) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kSigninAllowed, enable);
}

void InlineLoginUIBrowserTest::EnableOneClick(bool enable) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kReverseAutologinEnabled, enable);
}

void InlineLoginUIBrowserTest::AddEmailToOneClickRejectedList(
    const std::string& email) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  ListPrefUpdate updater(pref_service,
                         prefs::kReverseAutologinRejectedEmailList);
  updater->AppendIfNotPresent(new base::StringValue(email));
}

void InlineLoginUIBrowserTest::AllowSigninCookies(bool enable) {
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  cookie_settings->SetDefaultCookieSetting(enable ? CONTENT_SETTING_ALLOW
                                                  : CONTENT_SETTING_BLOCK);
}

void InlineLoginUIBrowserTest::SetAllowedUsernamePattern(
    const std::string& pattern) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kGoogleServicesUsernamePattern, pattern);
}

#if defined(OS_LINUX) || defined(OS_WIN)
// crbug.com/422868
#define MAYBE_DifferentStorageId DISABLED_DifferentStorageId
#else
#define MAYBE_DifferentStorageId DifferentStorageId
#endif
IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, MAYBE_DifferentStorageId) {
  ContentInfo info =
      NavigateAndGetInfo(browser(), GetSigninPromoURL(), CURRENT_TAB);
  WaitUntilUIReady(browser());

  // Make sure storage partition of embedded webview is different from
  // parent.
  std::set<content::WebContents*> set;
  GuestViewManager* manager = GuestViewManager::FromBrowserContext(
      info.contents->GetBrowserContext());
  manager->ForEachGuest(info.contents, base::Bind(&AddToSet, &set));
  ASSERT_EQ(1u, set.size());
  content::WebContents* webview_contents = *set.begin();
  content::RenderProcessHost* process =
      webview_contents->GetRenderProcessHost();
  ASSERT_NE(info.pid, process->GetID());
  ASSERT_NE(info.storage_partition, process->GetStoragePartition());
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, OneProcessLimit) {
  GURL test_url_1 = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html")));
  GURL test_url_2 = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("data:text/html,Hello world!")));

  // Even when the process limit is set to one, the signin process should
  // still be given its own process and storage partition.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  ContentInfo info1 =
      NavigateAndGetInfo(browser(), test_url_1, CURRENT_TAB);
  ContentInfo info2 =
      NavigateAndGetInfo(browser(), test_url_2, CURRENT_TAB);
  ContentInfo info3 =
      NavigateAndGetInfo(browser(), GetSigninPromoURL(), CURRENT_TAB);

  ASSERT_EQ(info1.pid, info2.pid);
  ASSERT_NE(info1.pid, info3.pid);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferNoProfile) {
  std::string error_message;
  EXPECT_FALSE(InlineLoginHandlerImpl::CanOffer(
      NULL, InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOffer) {
  EnableOneClick(true);
  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "user@gmail.com", NULL));

  EnableOneClick(false);

  std::string error_message;

  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "user@gmail.com", &error_message));
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferProfileConnected) {
  SetUpSigninManager("foo@gmail.com");
  EnableSigninAllowed(true);

  std::string error_message;

  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "foo@gmail.com", &error_message));
  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "foo", &error_message));
  EXPECT_FALSE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "user@gmail.com", &error_message));
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_SYNC_WRONG_EMAIL,
                                      base::UTF8ToUTF16("foo@gmail.com")),
            error_message);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferUsernameNotAllowed) {
  SetAllowedUsernamePattern("*.google.com");

  std::string error_message;
  EXPECT_FALSE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "foo@gmail.com", &error_message));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SYNC_LOGIN_NAME_PROHIBITED),
            error_message);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferWithRejectedEmail) {
  EnableSigninAllowed(true);

  AddEmailToOneClickRejectedList("foo@gmail.com");
  AddEmailToOneClickRejectedList("user@gmail.com");

  std::string error_message;
  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "foo@gmail.com", &error_message));
  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "user@gmail.com", &error_message));
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferNoSigninCookies) {
  AllowSigninCookies(false);
  EnableSigninAllowed(true);

  std::string error_message;
  EXPECT_FALSE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "12345", "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
}

class InlineLoginHelperBrowserTest : public InProcessBrowserTest {
 public:
  InlineLoginHelperBrowserTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(&InlineLoginHelperBrowserTest::
                               OnWillCreateBrowserContextServices,
                           base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Replace the signin manager and token service with fakes. Do this ahead of
    // creating the browser so that a bunch of classes don't register as
    // observers and end up needing to unregister when the fake is substituted.
    SigninManagerFactory::GetInstance()->SetTestingFactory(
        context, &BuildFakeSigninManagerBase);
    ProfileOAuth2TokenServiceFactory::GetInstance()->SetTestingFactory(
        context, &BuildFakeProfileOAuth2TokenService);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Grab references to the fake signin manager and token service.
    Profile* profile = browser()->profile();
    signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetInstance()->GetForProfile(profile));
    ASSERT_TRUE(signin_manager_);
    token_service_ = static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetInstance()->GetForProfile(
            profile));
    ASSERT_TRUE(token_service_);
  }

    void SimulateStartCookieForOAuthLoginTokenExchangeSuccess(
      const std::string& cookie_string) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    net::ResponseCookies cookies;
    cookies.push_back(cookie_string);
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(net::HTTP_OK);
    fetcher->set_cookies(cookies);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SimulateStartAuthCodeForOAuth2TokenExchangeSuccess(
      const std::string& json_response) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(net::HTTP_OK);
    fetcher->SetResponseString(json_response);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SimulateOnClientOAuthSuccess(GaiaAuthConsumer* consumer,
                                    const std::string& refresh_token) {
    GaiaAuthConsumer::ClientOAuthResult result;
    result.refresh_token = refresh_token;
    consumer->OnClientOAuthSuccess(result);
  }

  FakeSigninManagerForTesting* signin_manager() { return signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return token_service_; }

 private:
  net::TestURLFetcherFactory url_fetcher_factory_;
  FakeSigninManagerForTesting* signin_manager_;
  FakeProfileOAuth2TokenService* token_service_;
  scoped_ptr<base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginHelperBrowserTest);
};

// Test signin helper calls correct fetcher methods when called with a
// session index.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest, WithSessionIndex) {
  base::WeakPtr<InlineLoginHandlerImpl> handler;
  MockInlineSigninHelper helper(handler,
                                browser()->profile()->GetRequestContext(),
                                browser()->profile(),
                                GURL(),
                                "foo@gmail.com",
                                "gaiaid-12345",
                                "password",
                                "0",  // session index from above
                                std::string(),  // auth code
                                std::string(),
                                false,  // choose what to sync
                                false);  // confirm untrusted signin
  EXPECT_CALL(helper, OnClientOAuthSuccess(_));

  SimulateStartCookieForOAuthLoginTokenExchangeSuccess(
      "secure; httponly; oauth_code=code");
  SimulateStartAuthCodeForOAuth2TokenExchangeSuccess(
      "{\"access_token\": \"access_token\", \"expires_in\": 1234567890,"
      " \"refresh_token\": \"refresh_token\"}");
}

// Test signin helper calls correct fetcher methods when called with an
// auth code.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest, WithAuthCode) {
  base::WeakPtr<InlineLoginHandlerImpl> handler;
  MockInlineSigninHelper helper(handler,
                                browser()->profile()->GetRequestContext(),
                                browser()->profile(),
                                GURL(),
                                "foo@gmail.com",
                                "gaiaid-12345",
                                "password",
                                "",  // session index
                                "auth_code",  // auth code
                                std::string(),
                                false,  // choose what to sync
                                false);  // confirm untrusted signin
  EXPECT_CALL(helper, OnClientOAuthSuccess(_));

  SimulateStartAuthCodeForOAuth2TokenExchangeSuccess(
      "{\"access_token\": \"access_token\", \"expires_in\": 1234567890,"
      " \"refresh_token\": \"refresh_token\"}");
}

// Test signin helper creates sync starter with correct confirmation when
// signing in with default sync options.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       SigninCreatesSyncStarter1) {
  // See Source enum in components/signin/core/browser/signin_metrics.h for
  // possible values of access_point=, reason=.
  GURL url("chrome://chrome-signin/?access_point=0&reason=0");
  base::WeakPtr<InlineLoginHandlerImpl> handler;
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler,
          browser()->profile()->GetRequestContext(),
          browser()->profile(),
          url,
          "foo@gmail.com",
          "gaiaid-12345",
          "password",
          "",  // session index
          "auth_code",  // auth code
          std::string(),
          false,  // choose what to sync
          false);  // confirm untrusted signin
  EXPECT_CALL(
      *helper,
      CreateSyncStarter(_, _, _, _, "refresh_token",
                        OneClickSigninSyncStarter::CONFIRM_SYNC_SETTINGS_FIRST,
                        OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN));

  SimulateOnClientOAuthSuccess(helper, "refresh_token");
  base::MessageLoop::current()->RunUntilIdle();
}

// Test signin helper creates sync starter with correct confirmation when
// signing in and choosing what to sync first.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       SigninCreatesSyncStarter2) {
  // See Source enum in components/signin/core/browser/signin_metrics.h for
  // possible values of access_point=, reason=.
  const GURL url("chrome://chrome-signin/?access_point=0&reason=0");
  base::WeakPtr<InlineLoginHandlerImpl> handler;
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler,
          browser()->profile()->GetRequestContext(),
          browser()->profile(),
          url,
          "foo@gmail.com",
          "gaiaid-12345",
          "password",
          "",  // session index
          "auth_code",  // auth code
          std::string(),
          true,  // choose what to sync
          false);  // confirm untrusted signin
  EXPECT_CALL(*helper, CreateSyncStarter(
                           _, _, _, _, "refresh_token",
                           OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST,
                           OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN));

  SimulateOnClientOAuthSuccess(helper, "refresh_token");
  base::MessageLoop::current()->RunUntilIdle();
}

// Test signin helper creates sync starter with correct confirmation when
// signing in with an untrusted sign occurs.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       SigninCreatesSyncStarter3) {
  // See Source enum in components/signin/core/browser/signin_metrics.h for
  // possible values of access_point=, reason=.
  GURL url("chrome://chrome-signin/?access_point=0&reason=0");
  base::WeakPtr<InlineLoginHandlerImpl> handler;
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler,
          browser()->profile()->GetRequestContext(),
          browser()->profile(),
          url,
          "foo@gmail.com",
          "gaiaid-12345",
          "password",
          "",  // session index
          "auth_code",  // auth code
          std::string(),
          false,  // choose what to sync
          true);  // confirm untrusted signin
  EXPECT_CALL(
      *helper,
      CreateSyncStarter(_, _, _, _, "refresh_token",
                        OneClickSigninSyncStarter::CONFIRM_SYNC_SETTINGS_FIRST,
                        OneClickSigninSyncStarter::CONFIRM_UNTRUSTED_SIGNIN));

  SimulateOnClientOAuthSuccess(helper, "refresh_token");
  base::MessageLoop::current()->RunUntilIdle();
}

// Test signin helper creates sync starter with correct confirmation during
// re-auth.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       SigninCreatesSyncStarter4) {
  // See Source enum in components/signin/core/browser/signin_metrics.h for
  // possible values of access_point=, reason=.
  const GURL url("chrome://chrome-signin/?access_point=3&reason=0");
  base::WeakPtr<InlineLoginHandlerImpl> handler;
  // MockSyncStarterInlineSigninHelper will delete itself when done using
  // base::ThreadTaskRunnerHandle::DeleteSoon(), so need to delete here.  But
  // do need the RunUntilIdle() at the end.
  MockSyncStarterInlineSigninHelper* helper =
      new MockSyncStarterInlineSigninHelper(
          handler,
          browser()->profile()->GetRequestContext(),
          browser()->profile(),
          url,
          "foo@gmail.com",
          "gaiaid-12345",
          "password",
          "",  // session index
          "auth_code",  // auth code
          std::string(),
          false,  // choose what to sync
          false);  // confirm untrusted signin

  // Even though "choose what to sync" is false, the source of the URL is
  // settings, which means the user wants to CONFIGURE_SYNC_FIRST.
  EXPECT_CALL(*helper, CreateSyncStarter(
                           _, _, _, _, "refresh_token",
                           OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST,
                           OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN));

  SimulateOnClientOAuthSuccess(helper, "refresh_token");
  base::MessageLoop::current()->RunUntilIdle();
}

// Test signin helper does not create sync starter when reauthenticating.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       ReauthCallsUpdateCredentials) {
  ASSERT_EQ(0ul, token_service()->GetAccounts().size());

  // See Source enum in components/signin/core/browser/signin_metrics.h for
  // possible values of access_point=, reason=.
  GURL url("chrome://chrome-signin/?access_point=3&reason=2");
  base::WeakPtr<InlineLoginHandlerImpl> handler;
  InlineSigninHelper helper(handler,
                            browser()->profile()->GetRequestContext(),
                            browser()->profile(),
                            url,
                            "foo@gmail.com",
                            "gaiaid-12345",
                            "password",
                            "",  // session index
                            "auth_code",  // auth code
                            std::string(),
                            false,  // choose what to sync
                            false);  // confirm untrusted signin
  SimulateOnClientOAuthSuccess(&helper, "refresh_token");
  ASSERT_EQ(1ul, token_service()->GetAccounts().size());
  base::MessageLoop::current()->RunUntilIdle();
}

// Test signin helper does not create sync starter when adding another account
// to profile.
IN_PROC_BROWSER_TEST_F(InlineLoginHelperBrowserTest,
                       AddAccountsCallsUpdateCredentials) {
  ASSERT_EQ(0ul, token_service()->GetAccounts().size());

  // See Source enum in components/signin/core/browser/signin_metrics.h for
  // possible values of access_point=, reason=.
  GURL url("chrome://chrome-signin/?access_point=10&reason=1");
  base::WeakPtr<InlineLoginHandlerImpl> handler;
  InlineSigninHelper helper(handler,
                            browser()->profile()->GetRequestContext(),
                            browser()->profile(),
                            url,
                            "foo@gmail.com",
                            "gaiaid-12345",
                            "password",
                            "",  // session index
                            "auth_code",  // auth code
                            std::string(),
                            false,  // choose what to sync
                            false);  // confirm untrusted signin
  SimulateOnClientOAuthSuccess(&helper, "refresh_token");
  ASSERT_EQ(1ul, token_service()->GetAccounts().size());
  base::MessageLoop::current()->RunUntilIdle();
}

class InlineLoginUISafeIframeBrowserTest : public InProcessBrowserTest {
 public:
  FooWebUIProvider& foo_provider() { return foo_provider_; }

 private:
  void SetUp() override {
    // Don't spin up the IO thread yet since no threads are allowed while
    // spawning sandbox host process. See crbug.com/322732.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const GURL& base_url = embedded_test_server()->base_url();
    command_line->AppendSwitchASCII(::switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                    base_url.spec());
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();

    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        ChromeWebUIControllerFactory::GetInstance());
    test_factory_.reset(new TestChromeWebUIControllerFactory);
    content::WebUIControllerFactory::RegisterFactory(test_factory_.get());
    test_factory_->AddFactoryOverride(
        GURL(kFooWebUIURL).host(), &foo_provider_);
  }

  void TearDownOnMainThread() override {
    test_factory_->RemoveFactoryOverride(GURL(kFooWebUIURL).host());
    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        test_factory_.get());
    test_factory_.reset();
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  FooWebUIProvider foo_provider_;
  scoped_ptr<TestChromeWebUIControllerFactory> test_factory_;
};

// Make sure that the foo webui handler is working properly and that it gets
// created when navigated to normally.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest, Basic) {
  const GURL kUrl(kFooWebUIURL);
  EXPECT_CALL(foo_provider(), NewWebUI(_, ::testing::Eq(kUrl)))
      .WillOnce(ReturnNewWebUI());
  ui_test_utils::NavigateToURL(browser(), GURL(kFooWebUIURL));
}

// Make sure that the foo webui handler does not get created when we try to
// load it inside the iframe of the login ui.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest, NoWebUIInIframe) {
  GURL url = GetSigninPromoURL().Resolve(
      "?source=0&access_point=0&reason=0&frameUrl=chrome://foo");
  EXPECT_CALL(foo_provider(), NewWebUI(_, _)).Times(0);
  ui_test_utils::NavigateToURL(browser(), url);
}

// Make sure that the gaia iframe cannot trigger top-frame navigation.
// TODO(guohui): flaky on trybot crbug/364759.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest,
    TopFrameNavigationDisallowed) {
  // Loads into gaia iframe a web page that attempts to deframe on load.
  GURL deframe_url(embedded_test_server()->GetURL("/login/deframe.html"));
  GURL url(net::AppendOrReplaceQueryParameter(GetSigninPromoURL(), "frameUrl",
                                              deframe_url.spec()));
  ui_test_utils::NavigateToURL(browser(), url);
  WaitUntilUIReady(browser());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetVisibleURL());

  content::NavigationController& controller = contents->GetController();
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
}

// Flaky on CrOS, http://crbug.com/364759.
// Also flaky on Mac, http://crbug.com/442674.
// Also flaky on Linux which is just too flaky
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest,
    DISABLED_NavigationToOtherChromeURLDisallowed) {
  ui_test_utils::NavigateToURL(browser(), GetSigninPromoURL());
  WaitUntilUIReady(browser());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      contents, "window.location.href = 'chrome://foo'"));

  content::TestNavigationObserver navigation_observer(contents, 1);
  navigation_observer.Wait();

  EXPECT_EQ(GURL("about:blank"), contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest,
    ConfirmationRequiredForNonsecureSignin) {
  FakeGaia fake_gaia;
  fake_gaia.Initialize();

  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&FakeGaia::HandleRequest,
                 base::Unretained(&fake_gaia)));
  fake_gaia.SetFakeMergeSessionParams(
      "email@gmail.com", "fake-sid-cookie", "fake-lsid-cookie");

  // Navigates to the Chrome signin page which loads the fake gaia auth page.
  // Since the fake gaia auth page is served over HTTP, thus expects to see an
  // untrusted signin confirmation dialog upon submitting credentials below.
  ui_test_utils::NavigateToURL(browser(), GetSigninPromoURL());
  WaitUntilUIReady(browser());

  MockLoginUIObserver observer;
  LoginUIServiceFactory::GetForProfile(browser()->profile())
      ->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnUntrustedLoginUIShown())
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  ExecuteJsToSigninInSigninFrame(browser(), "email@gmail.com", "password");
  run_loop.Run();
  base::MessageLoop::current()->RunUntilIdle();
}
