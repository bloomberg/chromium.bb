// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/signin_partition_manager.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "media/base/media_switches.h"
#include "net/cookies/canonical_cookie.h"
#include "net/test/cert_test_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/interfaces/cookie_manager.mojom.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

constexpr char kTestCookieName[] = "TestCookie";
constexpr char kTestCookieValue[] = "present";
constexpr char kTestCookieHost[] = "host1.com";

void InjectCookieDoneCallback(base::OnceClosure done_closure, bool result) {
  ASSERT_TRUE(result);
  std::move(done_closure).Run();
}

// Injects a cookie into |storage_partition|, so we can test for cookie presence
// later to infer if the StoragePartition has been cleared.
void InjectCookie(content::StoragePartition* storage_partition) {
  network::mojom::CookieManagerPtr cookie_manager;
  storage_partition->GetNetworkContext()->GetCookieManager(
      mojo::MakeRequest(&cookie_manager));

  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      net::CanonicalCookie(kTestCookieName, kTestCookieValue, kTestCookieHost,
                           "/", base::Time(), base::Time(), base::Time(), false,
                           false, net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_MEDIUM),
      false, false,
      base::Bind(&InjectCookieDoneCallback, run_loop.QuitClosure()));
  run_loop.Run();
}

void GetAllCookiesCallback(std::string* cookies_out,
                           base::OnceClosure done_closure,
                           const std::vector<net::CanonicalCookie>& cookies) {
  *cookies_out = net::CanonicalCookie::BuildCookieLine(cookies);
  std::move(done_closure).Run();
}

// Returns all cookies present in |storage_partition| as a HTTP header cookie
// line. Will be an empty string if there are no cookies.
std::string GetAllCookies(content::StoragePartition* storage_partition) {
  network::mojom::CookieManagerPtr cookie_manager;
  storage_partition->GetNetworkContext()->GetCookieManager(
      mojo::MakeRequest(&cookie_manager));

  std::string cookies;
  base::RunLoop run_loop;
  cookie_manager->GetAllCookies(
      base::BindOnce(&GetAllCookiesCallback, &cookies, run_loop.QuitClosure()));
  run_loop.Run();
  return cookies;
}

// Spins the loop until a notification is received from |prefs| that the value
// of |pref_name| has changed. If the notification is received before Wait()
// has been called, Wait() returns immediately and no loop is spun.
class PrefChangeWatcher {
 public:
  PrefChangeWatcher(const std::string& pref_name, PrefService* prefs);

  void Wait();

 private:
  void OnPrefChange();

  bool pref_changed_ = false;

  base::RunLoop run_loop_;
  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefChangeWatcher);
};

PrefChangeWatcher::PrefChangeWatcher(const std::string& pref_name,
                                     PrefService* prefs) {
  registrar_.Init(prefs);
  registrar_.Add(pref_name, base::Bind(&PrefChangeWatcher::OnPrefChange,
                                       base::Unretained(this)));
}

void PrefChangeWatcher::Wait() {
  if (!pref_changed_)
    run_loop_.Run();
}

void PrefChangeWatcher::OnPrefChange() {
  pref_changed_ = true;
  run_loop_.Quit();
}

}  // namespace

class WebviewLoginTest : public OobeBaseTest {
 public:
  WebviewLoginTest() {}
  ~WebviewLoginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    command_line->AppendSwitch(::switches::kUseFakeDeviceForMediaStream);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

 protected:
  void ClickNext() {
    ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");
  }

  void ExpectIdentifierPage() {
    // First page: no back button, no close button, refresh button, #identifier
    // input field.
    JsExpect("!$('gaia-navigation').backVisible");
    JsExpect("!$('gaia-navigation').closeVisible");
    JsExpect("$('gaia-navigation').refreshVisible");
    JsExpect("$('signin-frame').src.indexOf('#identifier') != -1");
  }

  void ExpectPasswordPage() {
    // Second page: back button, close button, no refresh button,
    // #challengepassword input field.
    JsExpect("$('gaia-navigation').backVisible");
    JsExpect("$('gaia-navigation').closeVisible");
    JsExpect("!$('gaia-navigation').refreshVisible");
    JsExpect("$('signin-frame').src.indexOf('#challengepassword') != -1");
  }

  bool WebViewVisited(content::BrowserContext* browser_context,
                      content::StoragePartition* expected_storage_partition,
                      bool* out_web_view_found,
                      content::WebContents* guest_contents) {
    content::StoragePartition* guest_storage_partition =
        content::BrowserContext::GetStoragePartition(
            browser_context, guest_contents->GetSiteInstance());
    if (guest_storage_partition == expected_storage_partition)
      *out_web_view_found = true;

    // Returns true if found - this will exit the iteration early.
    return *out_web_view_found;
  }

  // Returns true if a webview which has a WebContents associated with
  // |storage_partition| currently exists in the login UI's main WebContents.
  bool IsLoginScreenHasWebviewWithStoragePartition(
      content::StoragePartition* storage_partition) {
    bool web_view_found = false;

    content::WebContents* web_contents = GetLoginUI()->GetWebContents();
    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    guest_view::GuestViewManager* guest_view_manager =
        guest_view::GuestViewManager::FromBrowserContext(browser_context);
    guest_view_manager->ForEachGuest(
        web_contents,
        base::Bind(&WebviewLoginTest::WebViewVisited, base::Unretained(this),
                   browser_context, storage_partition, &web_view_found));

    return web_view_found;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewLoginTest);
};

// Basic signin with username and password.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, Basic) {
  WaitForGaiaPageLoad();

  ExpectIdentifierPage();

  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ClickNext();
  ExpectPasswordPage();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ClickNext();

  session_start_waiter.Wait();
}

// Fails: http://crbug.com/512648.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, DISABLED_BackButton) {
  WaitForGaiaPageLoad();

  // Start with identifer page.
  ExpectIdentifierPage();

  // Move to password page.
  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ClickNext();
  ExpectPasswordPage();

  // Click back to identifier page.
  JS().Evaluate("$('gaia-navigation').$.backButton.click();");
  ExpectIdentifierPage();

  // Click next to password page, user id is remembered.
  ClickNext();
  ExpectPasswordPage();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  // Finish sign-up.
  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ClickNext();

  session_start_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, AllowGuest) {
  WaitForGaiaPageLoad();
  JsExpect("!$('guest-user-header-bar-item').hidden");
  CrosSettings::Get()->SetBoolean(kAccountsPrefAllowGuest, false);
  JsExpect("$('guest-user-header-bar-item').hidden");
}

// Create new account option should be available only if the settings allow it.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, AllowNewUser) {
  WaitForGaiaPageLoad();

  std::string frame_url = "$('gaia-signin').gaiaAuthHost_.reloadUrl_";
  // New users are allowed.
  JsExpect(frame_url + ".search('flow=nosignup') == -1");

  // Disallow new users - we also need to set a whitelist due to weird logic.
  CrosSettings::Get()->Set(kAccountsPrefUsers, base::ListValue());
  CrosSettings::Get()->SetBoolean(kAccountsPrefAllowNewUser, false);
  WaitForGaiaPageReload();

  // flow=nosignup indicates that user creation is not allowed.
  JsExpect(frame_url + ".search('flow=nosignup') != -1");
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, EmailPrefill) {
  WaitForGaiaPageLoad();
  JS().ExecuteAsync("Oobe.showSigninUI('user@example.com')");
  WaitForGaiaPageReload();
  EXPECT_EQ(fake_gaia_->prefilled_email(), "user@example.com");
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, StoragePartitionHandling) {
  WaitForGaiaPageLoad();

  // Start with identifer page.
  ExpectIdentifierPage();

  // WebContents of the embedding frame
  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();

  std::string signin_frame_partition_name_1 =
      JS().GetString("$('signin-frame').partition");
  content::StoragePartition* signin_frame_partition_1 =
      login::GetSigninPartition();

  EXPECT_FALSE(signin_frame_partition_name_1.empty());
  EXPECT_EQ(login::SigninPartitionManager::Factory::GetForBrowserContext(
                browser_context)
                ->GetCurrentStoragePartitionName(),
            signin_frame_partition_name_1);
  EXPECT_TRUE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_1));
  // Inject a cookie into the currently used StoragePartition, so we can test
  // later if it has been cleared.
  InjectCookie(signin_frame_partition_1);

  // Press the back button at a sign-in screen without pre-existing users to
  // start a new sign-in attempt.
  JS().Evaluate("$('signin-back-button').fire('tap')");
  WaitForGaiaPageReload();
  // Expect that we got back to the identifier page, as there are no known users
  // so the sign-in screen will not display user pods.
  ExpectIdentifierPage();

  std::string signin_frame_partition_name_2 =
      JS().GetString("$('signin-frame').partition");
  content::StoragePartition* signin_frame_partition_2 =
      login::GetSigninPartition();

  EXPECT_FALSE(signin_frame_partition_name_2.empty());
  EXPECT_EQ(login::SigninPartitionManager::Factory::GetForBrowserContext(
                browser_context)
                ->GetCurrentStoragePartitionName(),
            signin_frame_partition_name_2);
  EXPECT_TRUE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_2));
  InjectCookie(signin_frame_partition_2);

  // Make sure that the partitions differ and that the old one is not in use
  // anymore.
  EXPECT_NE(signin_frame_partition_name_1, signin_frame_partition_name_2);
  EXPECT_NE(signin_frame_partition_1, signin_frame_partition_2);
  EXPECT_FALSE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_1));

  // The StoragePartition which is not in use is supposed to have been cleared.
  EXPECT_EQ("", GetAllCookies(signin_frame_partition_1));
  EXPECT_NE("", GetAllCookies(signin_frame_partition_2));
}

// Tests that requesting webcam access from the login screen works correctly.
// This is needed for taking profile pictures.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, RequestCamera) {
  WaitForGaiaPageLoad();

  // Video devices should be allowed from the login screen.
  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  bool getUserMediaSuccess = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents->GetMainFrame(),
      "navigator.getUserMedia("
      "    {video: true},"
      "    function() { window.domAutomationController.send(true); },"
      "    function() { window.domAutomationController.send(false); });",
      &getUserMediaSuccess));
  EXPECT_TRUE(getUserMediaSuccess);

  // Audio devices should be denied from the login screen.
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents->GetMainFrame(),
      "navigator.getUserMedia("
      "    {audio: true},"
      "    function() { window.domAutomationController.send(true); },"
      "    function() { window.domAutomationController.send(false); });",
      &getUserMediaSuccess));
  EXPECT_FALSE(getUserMediaSuccess);
}

class WebviewClientCertsLoginTest : public WebviewLoginTest {
 public:
  WebviewClientCertsLoginTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableSigninFrameClientCertUserSelection);
    WebviewLoginTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    auto fake_session_manager_client =
        std::make_unique<FakeSessionManagerClient>();
    fake_session_manager_client_ = fake_session_manager_client.get();
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::move(fake_session_manager_client));
    device_policy_test_helper_.InstallOwnerKey();
    device_policy_test_helper_.MarkAsEnterpriseOwned();

    WebviewLoginTest::SetUpInProcessBrowserTestFixture();
  }

  // Installs a testing system slot and imports a client certificate into it.
  void SetUpClientCertInSystemSlot() {
    {
      bool system_slot_constructed_successfully = false;
      base::RunLoop loop;
      content::BrowserThread::PostTaskAndReply(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(&WebviewClientCertsLoginTest::SetUpTestSystemSlotOnIO,
                         base::Unretained(this),
                         &system_slot_constructed_successfully),
          loop.QuitClosure());
      loop.Run();
      ASSERT_TRUE(system_slot_constructed_successfully);
    }

    // Import a second client cert signed by another CA than client_1 into the
    // system wide key slot.
    client_cert_ = net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8",
        test_system_slot_->slot());
    ASSERT_TRUE(client_cert_.get());
  }

  // Sets up the DeviceLoginScreenAutoSelectCertificateForUrls policy.
  void SetAutoSelectCertificatePattern(const std::string& autoselect_pattern) {
    em::ChromeDeviceSettingsProto& proto(
        device_policy_test_helper_.device_policy()->payload());
    proto.mutable_device_login_screen_auto_select_certificate_for_urls()
        ->add_login_screen_auto_select_certificate_rules(autoselect_pattern);

    device_policy_test_helper_.device_policy()->Build();

    fake_session_manager_client_->set_device_policy(
        device_policy_test_helper_.device_policy()->GetBlob());
    PrefChangeWatcher watcher(prefs::kManagedAutoSelectCertificateForUrls,
                              ProfileHelper::GetSigninProfile()->GetPrefs());
    fake_session_manager_client_->OnPropertyChangeComplete(true);

    watcher.Wait();
  }

  // Starts the Test HTTPS server with |ssl_options|.
  void StartHttpsServer(const net::SpawnedTestServer::SSLOptions& ssl_options) {
    https_server_ = std::make_unique<net::SpawnedTestServer>(
        net::SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
    ASSERT_TRUE(https_server_->Start());
  }

  // Requests |http_server_|'s client-cert test page in the webview with the id
  // |webview_id|.  Returns the content of the client-cert test page.  If
  // |watch_new_webcontents| is true, this function will watch for newly-created
  // WebContents when determining if the navigation to the test page has
  // finished. This can be used for webviews which have not navigated yet, as
  // their WebContents will be created on-demand.
  std::string RequestClientCertTestPageInFrame(std::string webview_id,
                                               bool watch_new_webcontents) {
    const GURL url = https_server_->GetURL("client-cert");
    content::TestNavigationObserver navigation_observer(url);
    if (watch_new_webcontents)
      navigation_observer.StartWatchingNewWebContents();
    else
      navigation_observer.WatchExistingWebContents();

    JS().Evaluate(base::StringPrintf("$('%s').src='%s'", webview_id.c_str(),
                                     url.spec().c_str()));

    navigation_observer.Wait();

    content::WebContents* main_web_contents = GetLoginUI()->GetWebContents();
    content::WebContents* frame_web_contents =
        signin::GetAuthFrameWebContents(main_web_contents, webview_id);
    test::JSChecker frame_js_checker(frame_web_contents);
    const std::string https_reply_content =
        frame_js_checker.GetString("document.body.textContent");

    return https_reply_content;
  }

  void ShowEulaScreen() {
    LoginDisplayHost::default_host()->StartWizard(OobeScreen::SCREEN_OOBE_EULA);
    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  }

 private:
  void SetUpTestSystemSlotOnIO(bool* out_system_slot_constructed_successfully) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>();
    *out_system_slot_constructed_successfully =
        test_system_slot_->ConstructedSuccessfully();
  }

  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;
  // Unowned pointer - owned by DBusThreadManager.
  FakeSessionManagerClient* fake_session_manager_client_;
  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
  scoped_refptr<net::X509Certificate> client_cert_;
  std::unique_ptr<net::SpawnedTestServer> https_server_;

  DISALLOW_COPY_AND_ASSIGN(WebviewClientCertsLoginTest);
};

// Test that client certificate authentication using certificates from the
// system slot is enabled in the sign-in frame. The server does not request
// certificates signed by a specific authority.
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       SigninFrameNoAuthorityGiven) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::string autoselect_pattern =
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})";
  SetAutoSelectCertificatePattern(autoselect_pattern);

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);
  EXPECT_EQ(
      "got client cert with fingerprint: "
      "c66145f49caca4d1325db96ace0f12f615ba4981",
      https_reply_content);
}

// Test that if no client certificate is auto-selected using policy on the
// sign-in frame, the client does not send up any client certificate.
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       SigninFrameCertNotAutoSelected) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);

  EXPECT_EQ("got no client cert", https_reply_content);
}

// Test that client certificate authentication using certificates from the
// system slot is enabled in the sign-in frame. The server requests
// a certificate signed by a specific authority.
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest, SigninFrameAuthorityGiven) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  base::FilePath ca_path =
      net::GetTestCertsDirectory().Append(FILE_PATH_LITERAL("client_1_ca.pem"));
  ssl_options.client_authorities.push_back(ca_path);
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::string autoselect_pattern =
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})";
  SetAutoSelectCertificatePattern(autoselect_pattern);

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);
  EXPECT_EQ(
      "got client cert with fingerprint: "
      "c66145f49caca4d1325db96ace0f12f615ba4981",
      https_reply_content);
}

// Test that client certificate authentication using certificates from the
// system slot is enabled in the sign-in frame. The server requests
// a certificate signed by a specific authority. The client doesn't have a
// matching certificate.
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       SigninFrameAuthorityGivenNoMatchingCert) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  base::FilePath ca_path =
      net::GetTestCertsDirectory().Append(FILE_PATH_LITERAL("client_2_ca.pem"));
  ssl_options.client_authorities.push_back(ca_path);
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::string autoselect_pattern =
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})";
  SetAutoSelectCertificatePattern(autoselect_pattern);

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);
  EXPECT_EQ("got no client cert", https_reply_content);
}

// Tests that client certificate authentication is not enabled in a webview on
// the sign-in screen which is not the sign-in frame. In this case, the EULA
// webview is used.
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       ClientCertRequestedInOtherWebView) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::string autoselect_pattern =
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})";
  SetAutoSelectCertificatePattern(autoselect_pattern);

  ShowEulaScreen();

  // Use |watch_new_webcontents| because the EULA webview has not navigated yet.
  std::string https_reply_content = RequestClientCertTestPageInFrame(
      "cros-eula-frame", true /* watch_new_webcontents */);
  EXPECT_EQ("got no client cert", https_reply_content);
}

}  // namespace chromeos
