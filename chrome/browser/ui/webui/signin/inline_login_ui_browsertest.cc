// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/cookie_settings.h"
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
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

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
  signin_manager->SetAuthenticatedUsername(username);
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
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(browser()->profile()).get();
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
  if (switches::IsEnableWebviewBasedSignin()) {
    ContentInfo info = NavigateAndGetInfo(
        browser(),
        signin::GetPromoURL(signin_metrics::SOURCE_START_PAGE, false),
        CURRENT_TAB);
    WaitUntilUIReady(browser());

    // Make sure storage partition of embedded webview is different from
    // parent.
    std::set<content::WebContents*> set;
    extensions::GuestViewManager* manager =
        extensions::GuestViewManager::FromBrowserContext(
            info.contents->GetBrowserContext());
    manager->ForEachGuest(info.contents, base::Bind(&AddToSet, &set));
    ASSERT_EQ(1u, set.size());
    content::WebContents* webview_contents = *set.begin();
    content::RenderProcessHost* process =
        webview_contents->GetRenderProcessHost();
    ASSERT_NE(info.pid, process->GetID());
    ASSERT_NE(info.storage_partition, process->GetStoragePartition());
  } else {
    GURL test_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("title1.html")));

    ContentInfo info1 =
        NavigateAndGetInfo(browser(), test_url, CURRENT_TAB);
    ContentInfo info2 = NavigateAndGetInfo(
        browser(),
        signin::GetPromoURL(signin_metrics::SOURCE_START_PAGE, false),
        CURRENT_TAB);
    NavigateAndGetInfo(browser(), test_url, CURRENT_TAB);
    ContentInfo info3 = NavigateAndGetInfo(
        browser(),
        signin::GetPromoURL(signin_metrics::SOURCE_START_PAGE, false),
        NEW_FOREGROUND_TAB);

    // The info for signin should be the same.
    ASSERT_EQ(info2.storage_partition, info3.storage_partition);
    // The info for test_url and signin should be different.
    ASSERT_NE(info1.storage_partition, info2.storage_partition);
  }
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
  ContentInfo info3 = NavigateAndGetInfo(
      browser(),
      signin::GetPromoURL(signin_metrics::SOURCE_START_PAGE, false),
      CURRENT_TAB);

  ASSERT_EQ(info1.pid, info2.pid);
  ASSERT_NE(info1.pid, info3.pid);
}

#if !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferNoProfile) {
  std::string error_message;
  EXPECT_FALSE(InlineLoginHandlerImpl::CanOffer(
      NULL, InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOffer) {
  EnableOneClick(true);
  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "user@gmail.com", NULL));

  EnableOneClick(false);

  std::string error_message;

  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferProfileConnected) {
  SetUpSigninManager("foo@gmail.com");
  EnableSigninAllowed(true);

  std::string error_message;

  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "foo@gmail.com", &error_message));
  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "foo", &error_message));
  EXPECT_FALSE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_SYNC_WRONG_EMAIL,
                                      base::UTF8ToUTF16("foo@gmail.com")),
            error_message);
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferUsernameNotAllowed) {
  SetAllowedUsernamePattern("*.google.com");

  std::string error_message;
  EXPECT_FALSE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "foo@gmail.com", &error_message));
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
      "foo@gmail.com", &error_message));
  EXPECT_TRUE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
}

IN_PROC_BROWSER_TEST_F(InlineLoginUIBrowserTest, CanOfferNoSigninCookies) {
  AllowSigninCookies(false);
  EnableSigninAllowed(true);

  std::string error_message;
  EXPECT_FALSE(InlineLoginHandlerImpl::CanOffer(
      browser()->profile(), InlineLoginHandlerImpl::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
}

#endif  // OS_CHROMEOS

class InlineLoginUISafeIframeBrowserTest : public InProcessBrowserTest {
 public:
  FooWebUIProvider& foo_provider() { return foo_provider_; }

 private:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    // EmbeddedTestServer spawns a thread to initialize socket.
    // Stop IO thread in preparation for fork and exec.
    embedded_test_server()->StopThread();

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
    embedded_test_server()->RestartThreadAndListen();

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
  GURL url = signin::GetPromoURL(signin_metrics::SOURCE_START_PAGE, false).
      Resolve("?source=0&frameUrl=chrome://foo");
  EXPECT_CALL(foo_provider(), NewWebUI(_, _)).Times(0);
  ui_test_utils::NavigateToURL(browser(), url);
}

// Flaky on CrOS, http://crbug.com/364759.
#if defined(OS_CHROMEOS)
#define MAYBE_TopFrameNavigationDisallowed DISABLED_TopFrameNavigationDisallowed
#else
#define MAYBE_TopFrameNavigationDisallowed TopFrameNavigationDisallowed
#endif

// Make sure that the gaia iframe cannot trigger top-frame navigation.
// TODO(guohui): flaky on trybot crbug/364759.
IN_PROC_BROWSER_TEST_F(InlineLoginUISafeIframeBrowserTest,
    MAYBE_TopFrameNavigationDisallowed) {
  // Loads into gaia iframe a web page that attempts to deframe on load.
  GURL deframe_url(embedded_test_server()->GetURL("/login/deframe.html"));
  GURL url(net::AppendOrReplaceQueryParameter(
      signin::GetPromoURL(signin_metrics::SOURCE_START_PAGE, false),
      "frameUrl", deframe_url.spec()));
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
  ui_test_utils::NavigateToURL(
      browser(), signin::GetPromoURL(signin_metrics::SOURCE_START_PAGE, false));
  WaitUntilUIReady(browser());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      contents, "window.location.href = 'chrome://foo'"));

  content::TestNavigationObserver navigation_observer(contents, 1);
  navigation_observer.Wait();

  EXPECT_EQ(GURL("about:blank"), contents->GetVisibleURL());
}

#if !defined(OS_CHROMEOS)
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
  ui_test_utils::NavigateToURL(
      browser(), signin::GetPromoURL(signin_metrics::SOURCE_START_PAGE, false));
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
#endif // OS_CHROMEOS
