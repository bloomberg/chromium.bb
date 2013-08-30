// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/mock_login_status_consumer.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/policy_builder.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/proto/chromeos/chrome_device_policy.pb.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using testing::InvokeWithoutArgs;
using testing::Return;
using testing::_;

namespace policy {

namespace {

const char kDomain[] = "example.com";
const char kAccountId1[] = "dla1@example.com";
const char kAccountId2[] = "dla2@example.com";
const char kDisplayName[] = "display name";
const char* kStartupURLs[] = {
  "chrome://policy",
  "chrome://about",
};
const char kExistentTermsOfServicePath[] = "chromeos/enterprise/tos.txt";
const char kNonexistentTermsOfServicePath[] = "chromeos/enterprise/tos404.txt";

}  // namespace

class DeviceLocalAccountTest : public DevicePolicyCrosBrowserTest {
 protected:
  DeviceLocalAccountTest()
      : user_id_1_(GenerateDeviceLocalAccountUserId(
            kAccountId1, DeviceLocalAccount::TYPE_PUBLIC_SESSION)),
        user_id_2_(GenerateDeviceLocalAccountUserId(
            kAccountId2, DeviceLocalAccount::TYPE_PUBLIC_SESSION)) {}

  virtual ~DeviceLocalAccountTest() {}

  virtual void SetUp() OVERRIDE {
    // Configure and start the test server.
    scoped_ptr<crypto::RSAPrivateKey> signing_key(
        PolicyBuilder::CreateTestSigningKey());
    ASSERT_TRUE(test_server_.SetSigningKey(signing_key.get()));
    signing_key.reset();
    test_server_.RegisterClient(PolicyBuilder::kFakeToken,
                                PolicyBuilder::kFakeDeviceId);
    ASSERT_TRUE(test_server_.Start());

    DevicePolicyCrosBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(
        switches::kDeviceManagementUrl, test_server_.GetServiceURL().spec());
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Clear command-line arguments (but keep command-line switches) so the
    // startup pages policy takes effect.
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    CommandLine::StringVector argv(command_line->argv());
    argv.erase(argv.begin() + argv.size() - command_line->GetArgs().size(),
               argv.end());
    command_line->InitFromArgv(argv);

    InstallOwnerKey();
    MarkAsEnterpriseOwned();

    InitializePolicy();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // This shuts down the login UI.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(&chrome::AttemptExit));
    base::RunLoop().RunUntilIdle();
  }

  void InitializePolicy() {
    device_policy()->policy_data().set_public_key_version(1);
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_show_user_names()->set_show_user_names(true);

    device_local_account_policy_.policy_data().set_policy_type(
        dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy_.policy_data().set_username(kAccountId1);
    device_local_account_policy_.policy_data().set_settings_entity_id(
        kAccountId1);
    device_local_account_policy_.policy_data().set_public_key_version(1);
    device_local_account_policy_.payload().mutable_userdisplayname()->set_value(
        kDisplayName);
  }

  void BuildDeviceLocalAccountPolicy() {
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();
  }

  void UploadDeviceLocalAccountPolicy() {
    BuildDeviceLocalAccountPolicy();
    ASSERT_TRUE(session_manager_client()->device_local_account_policy(
        kAccountId1).empty());
    test_server_.UpdatePolicy(
        dm_protocol::kChromePublicAccountPolicyType, kAccountId1,
        device_local_account_policy_.payload().SerializeAsString());
  }

  void UploadAndInstallDeviceLocalAccountPolicy() {
    UploadDeviceLocalAccountPolicy();
    session_manager_client()->set_device_local_account_policy(
        kAccountId1, device_local_account_policy_.GetBlob());
  }

  void AddPublicSessionToDevicePolicy(const std::string& username) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    em::DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(username);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    RefreshDevicePolicy();
    test_server_.UpdatePolicy(dm_protocol::kChromeDevicePolicyType,
                              std::string(), proto.SerializeAsString());
  }

  void CheckPublicSessionPresent(const std::string& id) {
    const chromeos::User* user = chromeos::UserManager::Get()->FindUser(id);
    ASSERT_TRUE(user);
    EXPECT_EQ(id, user->email());
    EXPECT_EQ(chromeos::User::USER_TYPE_PUBLIC_ACCOUNT, user->GetType());
  }

  const std::string user_id_1_;
  const std::string user_id_2_;

  UserPolicyBuilder device_local_account_policy_;
  LocalPolicyTestServer test_server_;
};

static bool IsKnownUser(const std::string& account_id) {
  return chromeos::UserManager::Get()->IsKnownUser(account_id);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, LoginScreen) {
  AddPublicSessionToDevicePolicy(kAccountId1);
  AddPublicSessionToDevicePolicy(kAccountId2);

  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsKnownUser, user_id_1_))
      .Wait();
  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsKnownUser, user_id_2_))
      .Wait();

  CheckPublicSessionPresent(user_id_1_);
  CheckPublicSessionPresent(user_id_2_);
}

static bool DisplayNameMatches(const std::string& account_id,
                               const std::string& display_name) {
  const chromeos::User* user =
      chromeos::UserManager::Get()->FindUser(account_id);
  if (!user || user->display_name().empty())
    return false;
  EXPECT_EQ(UTF8ToUTF16(display_name), user->display_name());
  return true;
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DisplayName) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, user_id_1_, kDisplayName)).Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, PolicyDownload) {
  UploadDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  // Policy for the account is not installed in session_manager_client. Because
  // of this, the presence of the display name (which comes from policy) can be
  // used as a signal that indicates successful policy download.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, user_id_1_, kDisplayName)).Wait();

  // Sanity check: The policy should be present now.
  ASSERT_FALSE(session_manager_client()->device_local_account_policy(
      kAccountId1).empty());
}

static bool IsNotKnownUser(const std::string& account_id) {
  return !IsKnownUser(account_id);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DevicePolicyChange) {
  AddPublicSessionToDevicePolicy(kAccountId1);
  AddPublicSessionToDevicePolicy(kAccountId2);

  // Wait until the login screen is up.
  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsKnownUser, user_id_1_))
      .Wait();
  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsKnownUser, user_id_2_))
      .Wait();

  // Update policy to remove kAccountId2.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_device_local_accounts()->clear_account();
  AddPublicSessionToDevicePolicy(kAccountId1);

  em::ChromeDeviceSettingsProto policy;
  policy.mutable_show_user_names()->set_show_user_names(true);
  em::DeviceLocalAccountInfoProto* account1 =
      policy.mutable_device_local_accounts()->add_account();
  account1->set_account_id(kAccountId1);
  account1->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);

  test_server_.UpdatePolicy(dm_protocol::kChromeDevicePolicyType, std::string(),
                            policy.SerializeAsString());
  g_browser_process->policy_service()->RefreshPolicies(base::Closure());

  // Make sure the second device-local account disappears.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&IsNotKnownUser, user_id_2_)).Wait();
}

static bool IsSessionStarted() {
  return chromeos::UserManager::Get()->IsSessionStarted();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, StartSession) {
  // Specify startup pages.
  device_local_account_policy_.payload().mutable_restoreonstartup()->set_value(
      SessionStartupPref::kPrefValueURLs);
  em::StringListPolicyProto* startup_urls_proto =
      device_local_account_policy_.payload().mutable_restoreonstartupurls();
  for (size_t i = 0; i < arraysize(kStartupURLs); ++i)
    startup_urls_proto->mutable_value()->add_entries(kStartupURLs[i]);
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  // This observes the display name becoming available as this indicates
  // device-local account policy is fully loaded, which is a prerequisite for
  // successful login.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, user_id_1_, kDisplayName)).Wait();

  // Wait for the login UI to be ready.
  chromeos::LoginDisplayHostImpl* host =
      reinterpret_cast<chromeos::LoginDisplayHostImpl*>(
          chromeos::LoginDisplayHostImpl::default_host());
  ASSERT_TRUE(host);
  chromeos::OobeUI* oobe_ui = host->GetOobeUI();
  ASSERT_TRUE(oobe_ui);
  base::RunLoop run_loop;
  const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
  if (!oobe_ui_ready)
    run_loop.Run();

  // Start login into the device-local account.
  host->StartSignInScreen();
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
  controller->LoginAsPublicAccount(user_id_1_);

  // Wait for the session to start.
  content::WindowedNotificationObserver(chrome::NOTIFICATION_SESSION_STARTED,
                                        base::Bind(IsSessionStarted)).Wait();

  // Check that the startup pages specified in policy were opened.
  BrowserList* browser_list =
    BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  int expected_tab_count = static_cast<int>(arraysize(kStartupURLs));
  EXPECT_EQ(expected_tab_count, tabs->count());
  for (int i = 0; i < expected_tab_count && i < tabs->count(); ++i) {
    EXPECT_EQ(GURL(kStartupURLs[i]),
              tabs->GetWebContentsAt(i)->GetVisibleURL());
  }
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, FullscreenDisallowed) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  // This observes the display name becoming available as this indicates
  // device-local account policy is fully loaded, which is a prerequisite for
  // successful login.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, user_id_1_, kDisplayName)).Wait();

  // Wait for the login UI to be ready.
  chromeos::LoginDisplayHostImpl* host =
      reinterpret_cast<chromeos::LoginDisplayHostImpl*>(
          chromeos::LoginDisplayHostImpl::default_host());
  ASSERT_TRUE(host);
  chromeos::OobeUI* oobe_ui = host->GetOobeUI();
  ASSERT_TRUE(oobe_ui);
  base::RunLoop run_loop;
  const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
  if (!oobe_ui_ready)
    run_loop.Run();

  // Ensure that the browser stays alive, even though no windows are opened
  // during session start.
  chrome::StartKeepAlive();

  // Start login into the device-local account.
  host->StartSignInScreen();
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
  controller->LoginAsPublicAccount(user_id_1_);

  // Wait for the session to start.
  content::WindowedNotificationObserver(chrome::NOTIFICATION_SESSION_STARTED,
                                        base::Bind(IsSessionStarted)).Wait();

  // Open a browser window.
  chrome::NewEmptyWindow(ProfileManager::GetDefaultProfile(),
                         chrome::HOST_DESKTOP_TYPE_ASH);
  BrowserList* browser_list =
    BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  BrowserWindow* browser_window = browser->window();
  ASSERT_TRUE(browser_window);
  chrome::EndKeepAlive();

  // Verify that an attempt to enter fullscreen mode is denied.
  EXPECT_FALSE(browser_window->IsFullscreen());
  chrome::ToggleFullscreenMode(browser);
  EXPECT_FALSE(browser_window->IsFullscreen());
}

class TermsOfServiceTest : public DeviceLocalAccountTest,
                           public testing::WithParamInterface<bool> {
};

IN_PROC_BROWSER_TEST_P(TermsOfServiceTest, TermsOfServiceScreen) {
  // Specify Terms of Service URL.
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  device_local_account_policy_.payload().mutable_termsofserviceurl()->set_value(
      embedded_test_server()->GetURL(
            std::string("/") +
                (GetParam() ? kExistentTermsOfServicePath
                            : kNonexistentTermsOfServicePath)).spec());
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  // Wait for the device-local account policy to be fully loaded.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, user_id_1_, kDisplayName)).Wait();

  // Wait for the login UI to be ready.
  chromeos::LoginDisplayHostImpl* host =
      reinterpret_cast<chromeos::LoginDisplayHostImpl*>(
          chromeos::LoginDisplayHostImpl::default_host());
  ASSERT_TRUE(host);
  chromeos::OobeUI* oobe_ui = host->GetOobeUI();
  ASSERT_TRUE(oobe_ui);
  base::RunLoop oobe_ui_wait_run_loop;
  const bool oobe_ui_ready =
      oobe_ui->IsJSReady(oobe_ui_wait_run_loop.QuitClosure());
  if (!oobe_ui_ready)
    oobe_ui_wait_run_loop.Run();

  // Start login into the device-local account.
  host->StartSignInScreen();
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
  controller->LoginAsPublicAccount(user_id_1_);

  // Set up an observer that will quit the message loop when login has succeeded
  // and the first wizard screen, if any, is being shown.
  base::RunLoop login_wait_run_loop;
  chromeos::MockConsumer login_status_consumer;
  EXPECT_CALL(login_status_consumer, OnLoginSuccess(_, false, false))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(&login_wait_run_loop, &base::RunLoop::Quit));

  // Spin the loop until the observer fires. Then, unregister the observer.
  controller->set_login_status_consumer(&login_status_consumer);
  login_wait_run_loop.Run();
  controller->set_login_status_consumer(NULL);

  // Verify that the Terms of Service screen is being shown.
  chromeos::WizardController* wizard_controller =
        chromeos::WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  ASSERT_TRUE(wizard_controller->current_screen());
  EXPECT_EQ(chromeos::WizardController::kTermsOfServiceScreenName,
            wizard_controller->current_screen()->GetName());

  // Wait for the Terms of Service to finish downloading, then get the status of
  // the screen's UI elements.
  chromeos::WebUILoginView* web_ui_login_view = host->GetWebUILoginView();
  ASSERT_TRUE(web_ui_login_view);
  content::WebUI* web_ui = web_ui_login_view->GetWebUI();
  ASSERT_TRUE(web_ui);
  content::WebContents* contents = web_ui->GetWebContents();
  ASSERT_TRUE(contents);
  std::string json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents,
      "var screen = document.getElementById('terms-of-service');"
      "function SendReplyIfDownloadDone() {"
      "  if (screen.classList.contains('tos-loading'))"
      "    return false;"
      "  var status = {};"
      "  status.heading = document.getElementById('tos-heading').textContent;"
      "  status.subheading ="
      "      document.getElementById('tos-subheading').textContent;"
      "  status.contentHeading ="
      "      document.getElementById('tos-content-heading').textContent;"
      "  status.content ="
      "      document.getElementById('tos-content-main').textContent;"
      "  status.error = screen.classList.contains('error');"
      "  status.acceptEnabled ="
      "      !document.getElementById('tos-accept-button').disabled;"
      "  domAutomationController.send(JSON.stringify(status));"
      "  observer.disconnect();"
      "  return true;"
      "}"
      "var observer = new MutationObserver(SendReplyIfDownloadDone);"
      "if (!SendReplyIfDownloadDone()) {"
      "  var options = { attributes: true, attributeFilter: [ 'class' ] };"
      "  observer.observe(screen, options);"
      "}",
      &json));
  scoped_ptr<base::Value> value_ptr(base::JSONReader::Read(json));
  const base::DictionaryValue* status = NULL;
  ASSERT_TRUE(value_ptr.get());
  ASSERT_TRUE(value_ptr->GetAsDictionary(&status));
  std::string heading;
  EXPECT_TRUE(status->GetString("heading", &heading));
  std::string subheading;
  EXPECT_TRUE(status->GetString("subheading", &subheading));
  std::string content_heading;
  EXPECT_TRUE(status->GetString("contentHeading", &content_heading));
  std::string content;
  EXPECT_TRUE(status->GetString("content", &content));
  bool error;
  EXPECT_TRUE(status->GetBoolean("error", &error));
  bool accept_enabled;
  EXPECT_TRUE(status->GetBoolean("acceptEnabled", &accept_enabled));

  // Verify that the screen's headings have been set correctly.
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_TERMS_OF_SERVICE_SCREEN_HEADING,
                                UTF8ToUTF16(kDomain)),
      heading);
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_TERMS_OF_SERVICE_SCREEN_SUBHEADING,
                                UTF8ToUTF16(kDomain)),
      subheading);
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_TERMS_OF_SERVICE_SCREEN_CONTENT_HEADING,
                                UTF8ToUTF16(kDomain)),
      content_heading);

  if (!GetParam()) {
    // The Terms of Service URL was invalid. Verify that the screen is showing
    // an error and the accept button is disabled.
    EXPECT_TRUE(error);
    EXPECT_FALSE(accept_enabled);
    return;
  }

  // The Terms of Service URL was valid. Verify that the screen is showing the
  // downloaded Terms of Service and the accept button is enabled.
  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  std::string terms_of_service;
  ASSERT_TRUE(base::ReadFileToString(
      test_dir.Append(kExistentTermsOfServicePath), &terms_of_service));
  EXPECT_EQ(terms_of_service, content);
  EXPECT_FALSE(error);
  EXPECT_TRUE(accept_enabled);

  // Click the accept button.
  ASSERT_TRUE(content::ExecuteScript(contents,
                                     "$('tos-accept-button').click();"));

  // Wait for the session to start.
  if (!IsSessionStarted()) {
    content::WindowedNotificationObserver(chrome::NOTIFICATION_SESSION_STARTED,
                                          base::Bind(IsSessionStarted)).Wait();
  }
}

INSTANTIATE_TEST_CASE_P(TermsOfServiceTestInstance,
                        TermsOfServiceTest, testing::Bool());

}  // namespace policy
