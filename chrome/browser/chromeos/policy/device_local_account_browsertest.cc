// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "ash/shell.h"
#include "ash/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/system/chromeos/session/logout_confirmation_dialog.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"
#include "chrome/browser/chromeos/extensions/external_cache.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_test_util.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_descriptor.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/login/auth/mock_auth_status_consumer.h"
#include "chromeos/login/auth/user_context.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
//#include "third_party/cros_system_api/dbus/service_constants.h"

namespace em = enterprise_management;

using chromeos::LoginScreenContext;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::_;

namespace policy {

namespace {

const char kDomain[] = "example.com";
const char kAccountId1[] = "dla1@example.com";
const char kAccountId2[] = "dla2@example.com";
const char kDisplayName1[] = "display name 1";
const char kDisplayName2[] = "display name 2";
const char* kStartupURLs[] = {
  "chrome://policy",
  "chrome://about",
};
const char kExistentTermsOfServicePath[] = "chromeos/enterprise/tos.txt";
const char kNonexistentTermsOfServicePath[] = "chromeos/enterprise/tos404.txt";
const char kRelativeUpdateURL[] = "/service/update2/crx";
const char kUpdateManifestHeader[] =
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>\n";
const char kUpdateManifestTemplate[] =
    "  <app appid='%s'>\n"
    "    <updatecheck codebase='%s' version='%s' />\n"
    "  </app>\n";
const char kUpdateManifestFooter[] =
    "</gupdate>\n";
const char kHostedAppID[] = "kbmnembihfiondgfjekmnmcbddelicoi";
const char kHostedAppCRXPath[] = "extensions/hosted_app.crx";
const char kHostedAppVersion[] = "1.0.0.0";
const char kGoodExtensionID[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kGoodExtensionCRXPath[] = "extensions/good.crx";
const char kGoodExtensionVersion[] = "1.0";
const char kPackagedAppCRXPath[] = "extensions/platform_apps/app_window_2.crx";

const char kExternalData[] = "External data";
const char kExternalDataURL[] = "http://localhost/external_data";

const char kPublicSessionLocale[] = "de";
const char kPublicSessionInputMethodIDTemplate[] = "_comp_ime_%sxkb:de:neo:ger";

// Helper that serves extension update manifests to Chrome.
class TestingUpdateManifestProvider {
 public:
  // Update manifests will be served at |relative_update_url|.
  explicit TestingUpdateManifestProvider(
      const std::string& relative_update_url);
  ~TestingUpdateManifestProvider();

  // When an update manifest is requested for the given extension |id|, indicate
  // that |version| of the extension can be downloaded at |crx_url|.
  void AddUpdate(const std::string& id,
                 const std::string& version,
                 const GURL& crx_url);

  // This method must be registered with the test's EmbeddedTestServer to start
  // serving update manifests.
  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

 private:
  struct Update {
   public:
    Update(const std::string& version, const GURL& crx_url);
    Update();

    std::string version;
    GURL crx_url;
  };
  typedef std::map<std::string, Update> UpdateMap;
  UpdateMap updates_;

  const std::string relative_update_url_;

  DISALLOW_COPY_AND_ASSIGN(TestingUpdateManifestProvider);
};

// Helper that observes the dictionary |pref| in local state and waits until the
// value stored for |key| matches |expected_value|.
class DictionaryPrefValueWaiter {
 public:
  DictionaryPrefValueWaiter(const std::string& pref,
                            const std::string& key,
                            const std::string& expected_value);
  ~DictionaryPrefValueWaiter();

  void Wait();

 private:
  void QuitLoopIfExpectedValueFound();

  const std::string pref_;
  const std::string key_;
  const std::string expected_value_;

  base::RunLoop run_loop_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryPrefValueWaiter);
};

TestingUpdateManifestProvider::Update::Update(const std::string& version,
                                              const GURL& crx_url)
    : version(version),
      crx_url(crx_url) {
}

TestingUpdateManifestProvider::Update::Update() {
}

TestingUpdateManifestProvider::TestingUpdateManifestProvider(
    const std::string& relative_update_url)
    : relative_update_url_(relative_update_url) {
}

TestingUpdateManifestProvider::~TestingUpdateManifestProvider() {
}

void TestingUpdateManifestProvider::AddUpdate(const std::string& id,
                                              const std::string& version,
                                              const GURL& crx_url) {
  updates_[id] = Update(version, crx_url);
}

scoped_ptr<net::test_server::HttpResponse>
    TestingUpdateManifestProvider::HandleRequest(
        const net::test_server::HttpRequest& request) {
  const GURL url("http://localhost" + request.relative_url);
  if (url.path() != relative_update_url_)
    return scoped_ptr<net::test_server::HttpResponse>();

  std::string content = kUpdateManifestHeader;
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() != "x")
      continue;
    // Extract the extension id from the subquery. Since GetValueForKeyInQuery()
    // expects a complete URL, dummy scheme and host must be prepended.
    std::string id;
    net::GetValueForKeyInQuery(GURL("http://dummy?" + it.GetUnescapedValue()),
                               "id", &id);
    UpdateMap::const_iterator entry = updates_.find(id);
    if (entry != updates_.end()) {
      content += base::StringPrintf(kUpdateManifestTemplate,
                                    id.c_str(),
                                    entry->second.crx_url.spec().c_str(),
                                    entry->second.version.c_str());
    }
  }
  content += kUpdateManifestFooter;
  scoped_ptr<net::test_server::BasicHttpResponse>
      http_response(new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(content);
  http_response->set_content_type("text/xml");
  return http_response.PassAs<net::test_server::HttpResponse>();
}

DictionaryPrefValueWaiter::DictionaryPrefValueWaiter(
    const std::string& pref,
    const std::string& key,
    const std::string& expected_value)
    : pref_(pref),
      key_(key),
      expected_value_(expected_value) {
  pref_change_registrar_.Init(g_browser_process->local_state());
}

DictionaryPrefValueWaiter::~DictionaryPrefValueWaiter() {
}

void DictionaryPrefValueWaiter::Wait() {
  pref_change_registrar_.Add(
      pref_.c_str(),
      base::Bind(&DictionaryPrefValueWaiter::QuitLoopIfExpectedValueFound,
                 base::Unretained(this)));
  QuitLoopIfExpectedValueFound();
  run_loop_.Run();
}

void DictionaryPrefValueWaiter::QuitLoopIfExpectedValueFound() {
  const base::DictionaryValue* pref =
      pref_change_registrar_.prefs()->GetDictionary(pref_.c_str());
  ASSERT_TRUE(pref);
  std::string actual_value;
  if (pref->GetStringWithoutPathExpansion(key_, &actual_value) &&
      actual_value == expected_value_) {
    run_loop_.Quit();
  }
}

bool DoesInstallSuccessReferToId(const std::string& id,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  return content::Details<const extensions::InstalledExtensionInfo>(details)->
      extension->id() == id;
}

bool DoesInstallFailureReferToId(const std::string& id,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  return content::Details<const base::string16>(details)->
      find(base::UTF8ToUTF16(id)) != base::string16::npos;
}

scoped_ptr<net::FakeURLFetcher> RunCallbackAndReturnFakeURLFetcher(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Closure& callback,
    const GURL& url,
    net::URLFetcherDelegate* delegate,
    const std::string& response_data,
    net::HttpStatusCode response_code,
    net::URLRequestStatus::Status status) {
  task_runner->PostTask(FROM_HERE, callback);
  return make_scoped_ptr(new net::FakeURLFetcher(
      url, delegate, response_data, response_code, status));
}

bool IsSessionStarted() {
  return chromeos::UserManager::Get()->IsSessionStarted();
}

}  // namespace

class DeviceLocalAccountTest : public DevicePolicyCrosBrowserTest,
                               public chromeos::UserManager::Observer,
                               public chrome::BrowserListObserver,
                               public apps::AppWindowRegistry::Observer {
 protected:
  DeviceLocalAccountTest()
      : user_id_1_(GenerateDeviceLocalAccountUserId(
            kAccountId1, DeviceLocalAccount::TYPE_PUBLIC_SESSION)),
        user_id_2_(GenerateDeviceLocalAccountUserId(
            kAccountId2, DeviceLocalAccount::TYPE_PUBLIC_SESSION)),
        public_session_input_method_id_(base::StringPrintf(
            kPublicSessionInputMethodIDTemplate,
            chromeos::extension_ime_util::kXkbExtensionId)) {
    set_exit_when_last_browser_closes(false);
  }

  virtual ~DeviceLocalAccountTest() {}

  virtual void SetUp() OVERRIDE {
    // Configure and start the test server.
    scoped_ptr<crypto::RSAPrivateKey> signing_key(
        PolicyBuilder::CreateTestSigningKey());
    ASSERT_TRUE(test_server_.SetSigningKeyAndSignature(
        signing_key.get(), PolicyBuilder::GetTestSigningKeySignature()));
    signing_key.reset();
    test_server_.RegisterClient(PolicyBuilder::kFakeToken,
                                PolicyBuilder::kFakeDeviceId);
    ASSERT_TRUE(test_server_.Start());

    BrowserList::AddObserver(this);

    DevicePolicyCrosBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    test_server_.GetServiceURL().spec());
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

  virtual void TearDownOnMainThread() OVERRIDE {
    BrowserList::RemoveObserver(this);

    // This shuts down the login UI.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(&chrome::AttemptExit));
    base::RunLoop().RunUntilIdle();
  }

  virtual void LocalStateChanged(chromeos::UserManager* user_manager) OVERRIDE {
    if (run_loop_)
      run_loop_->Quit();
  }

  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE {
    if (run_loop_)
      run_loop_->Quit();
  }

  virtual void OnAppWindowAdded(apps::AppWindow* app_window) OVERRIDE {
    if (run_loop_)
      run_loop_->Quit();
  }

  virtual void OnAppWindowRemoved(apps::AppWindow* app_window) OVERRIDE {
    if (run_loop_)
      run_loop_->Quit();
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
        kDisplayName1);
  }

  void BuildDeviceLocalAccountPolicy() {
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();
  }

  void UploadDeviceLocalAccountPolicy() {
    BuildDeviceLocalAccountPolicy();
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
    const user_manager::User* user = chromeos::UserManager::Get()->FindUser(id);
    ASSERT_TRUE(user);
    EXPECT_EQ(id, user->email());
    EXPECT_EQ(user_manager::USER_TYPE_PUBLIC_ACCOUNT, user->GetType());
  }

  base::FilePath GetExtensionCacheDirectoryForAccountID(
      const std::string& account_id) {
    base::FilePath extension_cache_root_dir;
    if (!PathService::Get(chromeos::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS,
                          &extension_cache_root_dir)) {
      ADD_FAILURE();
    }
    return extension_cache_root_dir.Append(
        base::HexEncode(account_id.c_str(), account_id.size()));
  }

  base::FilePath GetCacheCRXFile(const std::string& account_id,
                                 const std::string& id,
                                 const std::string& version) {
    return GetExtensionCacheDirectoryForAccountID(account_id)
        .Append(base::StringPrintf("%s-%s.crx", id.c_str(), version.c_str()));
  }

  // Returns a profile which can be used for testing.
  Profile* GetProfileForTest() {
    // Any profile can be used here since this test does not test multi profile.
    return ProfileManager::GetActiveUserProfile();
  }

  void WaitForDisplayName(const std::string& user_id,
                          const std::string& expected_display_name) {
    DictionaryPrefValueWaiter("UserDisplayName",
                              user_id,
                              expected_display_name).Wait();
  }

  void WaitForPolicy() {
    // Wait for the display name becoming available as that indicates
    // device-local account policy is fully loaded, which is a prerequisite for
    // successful login.
    WaitForDisplayName(user_id_1_, kDisplayName1);
  }

  void GetWebContents(content::WebContents** contents) {
    chromeos::LoginDisplayHostImpl* host =
        reinterpret_cast<chromeos::LoginDisplayHostImpl*>(
            chromeos::LoginDisplayHostImpl::default_host());
    ASSERT_TRUE(host);
    chromeos::WebUILoginView* web_ui_login_view = host->GetWebUILoginView();
    ASSERT_TRUE(web_ui_login_view);
    content::WebUI* web_ui = web_ui_login_view->GetWebUI();
    ASSERT_TRUE(web_ui);
    *contents = web_ui->GetWebContents();
    ASSERT_TRUE(*contents);
  }

  void WaitForLoginUI() {
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

    // The network selection screen changes the application locale on load and
    // once again on blur. Wait for the screen to load and blur it so that any
    // locale changes caused by this screen happen now and do not affect any
    // subsequent parts of the test.
    content::WebContents* contents = NULL;
    ASSERT_NO_FATAL_FAILURE(GetWebContents(&contents));
    bool done = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "var languageSelect = document.getElementById('language-select');"
        "var blurAndReportSuccess = function() {"
        "  languageSelect.blur();"
        "  domAutomationController.send(true);"
        "};"
        "if (document.activeElement == languageSelect)"
        "  blurAndReportSuccess();"
        "else"
        "  languageSelect.addEventListener('focus', blurAndReportSuccess);",
        &done));
  }

  void StartLogin(const std::string& locale,
                  const std::string& input_method) {
    // Start login into the device-local account.
    chromeos::LoginDisplayHostImpl* host =
        reinterpret_cast<chromeos::LoginDisplayHostImpl*>(
            chromeos::LoginDisplayHostImpl::default_host());
    ASSERT_TRUE(host);
    host->StartSignInScreen(LoginScreenContext());
    chromeos::ExistingUserController* controller =
        chromeos::ExistingUserController::current_controller();
    ASSERT_TRUE(controller);

    chromeos::UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                       user_id_1_);
    user_context.SetPublicSessionLocale(locale);
    user_context.SetPublicSessionInputMethod(input_method);
    controller->LoginAsPublicSession(user_context);
  }

  void WaitForSessionStart() {
    if (IsSessionStarted())
      return;
    content::WindowedNotificationObserver(chrome::NOTIFICATION_SESSION_STARTED,
                                          base::Bind(IsSessionStarted)).Wait();
  }

  const std::string user_id_1_;
  const std::string user_id_2_;
  const std::string public_session_input_method_id_;

  scoped_ptr<base::RunLoop> run_loop_;

  UserPolicyBuilder device_local_account_policy_;
  LocalPolicyTestServer test_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountTest);
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
  EXPECT_TRUE(IsKnownUser(user_id_2_));

  CheckPublicSessionPresent(user_id_1_);
  CheckPublicSessionPresent(user_id_2_);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DisplayName) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Skip to the login screen.
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();

  // Verify that the display name is shown in the UI.
  content::WebContents* contents = NULL;
  ASSERT_NO_FATAL_FAILURE(GetWebContents(&contents));
  const std::string get_compact_pod_display_name = base::StringPrintf(
      "domAutomationController.send(document.getElementById('pod-row')"
      "    .getPodWithUsername_('%s').nameElement.textContent);",
      user_id_1_.c_str());
  std::string display_name;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents,
      get_compact_pod_display_name,
      &display_name));
  EXPECT_EQ(kDisplayName1, display_name);
  const std::string get_expanded_pod_display_name = base::StringPrintf(
      "domAutomationController.send(document.getElementById('pod-row')"
      "    .getPodWithUsername_('%s').querySelector('.expanded-pane-name')"
      "        .textContent);",
      user_id_1_.c_str());
  display_name.clear();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents,
      get_expanded_pod_display_name,
      &display_name));
  EXPECT_EQ(kDisplayName1, display_name);

  // Click on the pod to expand it.
  ASSERT_TRUE(content::ExecuteScript(
      contents,
      base::StringPrintf(
          "document.getElementById('pod-row').getPodWithUsername_('%s')"
          "    .click();",
      user_id_1_.c_str())));

  // Change the display name.
  device_local_account_policy_.payload().mutable_userdisplayname()->set_value(
      kDisplayName2);
  UploadAndInstallDeviceLocalAccountPolicy();
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceLocalAccountPolicyBroker* broker =
      connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
          user_id_1_);
  ASSERT_TRUE(broker);
  broker->core()->store()->Load();
  WaitForDisplayName(user_id_1_, kDisplayName2);

  // Verify that the new display name is shown in the UI.
  display_name.clear();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents,
      get_compact_pod_display_name,
      &display_name));
  EXPECT_EQ(kDisplayName2, display_name);
  display_name.clear();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents,
      get_expanded_pod_display_name,
      &display_name));
  EXPECT_EQ(kDisplayName2, display_name);

  // Verify that the pod is still expanded. This indicates that the UI updated
  // without reloading and losing state.
  bool expanded = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      base::StringPrintf(
          "domAutomationController.send(document.getElementById('pod-row')"
          "    .getPodWithUsername_('%s').expanded);",
          user_id_1_.c_str()),
      &expanded));
  EXPECT_TRUE(expanded);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, PolicyDownload) {
  UploadDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Sanity check: The policy should be present now.
  ASSERT_FALSE(session_manager_client()->device_local_account_policy(
      kAccountId1).empty());
}

static bool IsNotKnownUser(const std::string& account_id) {
  return !IsKnownUser(account_id);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, AccountListChange) {
  AddPublicSessionToDevicePolicy(kAccountId1);
  AddPublicSessionToDevicePolicy(kAccountId2);

  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsKnownUser, user_id_1_))
      .Wait();
  EXPECT_TRUE(IsKnownUser(user_id_2_));

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
  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsNotKnownUser, user_id_2_))
      .Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DISABLED_StartSession) {
  // Specify startup pages.
  device_local_account_policy_.payload().mutable_restoreonstartup()->set_value(
      SessionStartupPref::kPrefValueURLs);
  em::StringListPolicyProto* startup_urls_proto =
      device_local_account_policy_.payload().mutable_restoreonstartupurls();
  for (size_t i = 0; i < arraysize(kStartupURLs); ++i)
    startup_urls_proto->mutable_value()->add_entries(kStartupURLs[i]);
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

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

  // Verify that the session is not considered to be logged in with a GAIA
  // account.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->GetPrefs()->HasPrefPath(
      prefs::kGoogleServicesUsername));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DISABLED_FullscreenDisallowed) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  BrowserList* browser_list =
    BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  BrowserWindow* browser_window = browser->window();
  ASSERT_TRUE(browser_window);

  // Verify that an attempt to enter fullscreen mode is denied.
  EXPECT_FALSE(browser_window->IsFullscreen());
  chrome::ToggleFullscreenMode(browser);
  EXPECT_FALSE(browser_window->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DISABLED_ExtensionsUncached) {
  // Make it possible to force-install a hosted app and an extension.
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  TestingUpdateManifestProvider testing_update_manifest_provider(
      kRelativeUpdateURL);
  testing_update_manifest_provider.AddUpdate(
      kHostedAppID,
      kHostedAppVersion,
      embedded_test_server()->GetURL(std::string("/") + kHostedAppCRXPath));
  testing_update_manifest_provider.AddUpdate(
      kGoodExtensionID,
      kGoodExtensionVersion,
      embedded_test_server()->GetURL(std::string("/") + kGoodExtensionCRXPath));
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&TestingUpdateManifestProvider::HandleRequest,
                 base::Unretained(&testing_update_manifest_provider)));

  // Specify policy to force-install the hosted app and the extension.
  em::StringList* forcelist = device_local_account_policy_.payload()
      .mutable_extensioninstallforcelist()->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kHostedAppID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kGoodExtensionID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());

  // Start listening for app/extension installation results.
  content::WindowedNotificationObserver hosted_app_observer(
      extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      base::Bind(DoesInstallSuccessReferToId, kHostedAppID));
  content::WindowedNotificationObserver extension_observer(
      extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      base::Bind(DoesInstallFailureReferToId, kGoodExtensionID));

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Wait for the hosted app installation to succeed and the extension
  // installation to fail (because hosted apps are whitelisted for use in
  // device-local accounts and extensions are not).
  hosted_app_observer.Wait();
  extension_observer.Wait();

  // Verify that the hosted app was installed.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  EXPECT_TRUE(extension_service->GetExtensionById(kHostedAppID, true));

  // Verify that the extension was not installed.
  EXPECT_FALSE(extension_service->GetExtensionById(kGoodExtensionID, true));

  // Verify that the app was downloaded to the account's extension cache.
  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  EXPECT_TRUE(ContentsEqual(
          GetCacheCRXFile(kAccountId1, kHostedAppID, kHostedAppVersion),
          test_dir.Append(kHostedAppCRXPath)));

  // Verify that the extension was removed from the account's extension cache
  // after the installation failure.
  DeviceLocalAccountPolicyBroker* broker =
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetDeviceLocalAccountPolicyService()->GetBrokerForUser(user_id_1_);
  ASSERT_TRUE(broker);
  chromeos::ExternalCache* cache =
      broker->extension_loader()->GetExternalCacheForTesting();
  ASSERT_TRUE(cache);
  EXPECT_FALSE(cache->GetExtension(kGoodExtensionID, NULL, NULL));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DISABLED_ExtensionsCached) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Pre-populate the device local account's extension cache with a hosted app
  // and an extension.
  EXPECT_TRUE(base::CreateDirectory(
      GetExtensionCacheDirectoryForAccountID(kAccountId1)));
  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  const base::FilePath cached_hosted_app =
      GetCacheCRXFile(kAccountId1, kHostedAppID, kHostedAppVersion);
  EXPECT_TRUE(CopyFile(test_dir.Append(kHostedAppCRXPath),
                       cached_hosted_app));
  EXPECT_TRUE(CopyFile(
      test_dir.Append(kGoodExtensionCRXPath),
      GetCacheCRXFile(kAccountId1, kGoodExtensionID, kGoodExtensionVersion)));

  // Specify policy to force-install the hosted app.
  em::StringList* forcelist = device_local_account_policy_.payload()
      .mutable_extensioninstallforcelist()->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kHostedAppID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kGoodExtensionID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());

  // Start listening for app/extension installation results.
  content::WindowedNotificationObserver hosted_app_observer(
      extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      base::Bind(DoesInstallSuccessReferToId, kHostedAppID));
  content::WindowedNotificationObserver extension_observer(
      extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      base::Bind(DoesInstallFailureReferToId, kGoodExtensionID));

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Wait for the hosted app installation to succeed and the extension
  // installation to fail.
  hosted_app_observer.Wait();
  extension_observer.Wait();

  // Verify that the hosted app was installed.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  EXPECT_TRUE(extension_service->GetExtensionById(kHostedAppID, true));

  // Verify that the extension was not installed.
  EXPECT_FALSE(extension_service->GetExtensionById(kGoodExtensionID, true));

  // Verify that the app is still in the account's extension cache.
  EXPECT_TRUE(PathExists(cached_hosted_app));

  // Verify that the extension was removed from the account's extension cache.
  DeviceLocalAccountPolicyBroker* broker =
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetDeviceLocalAccountPolicyService()->GetBrokerForUser(user_id_1_);
  ASSERT_TRUE(broker);
  chromeos::ExternalCache* cache =
      broker->extension_loader()->GetExternalCacheForTesting();
  ASSERT_TRUE(cache);
  EXPECT_FALSE(cache->GetExtension(kGoodExtensionID, NULL, NULL));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DISABLED_ExternalData) {
  // chromeos::UserManager requests an external data fetch whenever
  // the key::kUserAvatarImage policy is set. Since this test wants to
  // verify that the underlying policy subsystem will start a fetch
  // without this request as well, the chromeos::UserManager must be
  // prevented from seeing the policy change.
  reinterpret_cast<chromeos::ChromeUserManager*>(chromeos::UserManager::Get())
      ->StopPolicyObserverForTesting();

  UploadDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Start serving external data at |kExternalDataURL|.
  scoped_ptr<base::RunLoop> run_loop(new base::RunLoop);
  scoped_ptr<net::FakeURLFetcherFactory> fetcher_factory(
      new net::FakeURLFetcherFactory(
          NULL,
          base::Bind(&RunCallbackAndReturnFakeURLFetcher,
                     base::MessageLoopProxy::current(),
                     run_loop->QuitClosure())));
  fetcher_factory->SetFakeResponse(GURL(kExternalDataURL),
                                   kExternalData,
                                   net::HTTP_OK,
                                   net::URLRequestStatus::SUCCESS);

  // Specify an external data reference for the key::kUserAvatarImage policy.
  scoped_ptr<base::DictionaryValue> metadata =
      test::ConstructExternalDataReference(kExternalDataURL, kExternalData);
  std::string policy;
  base::JSONWriter::Write(metadata.get(), &policy);
  device_local_account_policy_.payload().mutable_useravatarimage()->set_value(
      policy);
  UploadAndInstallDeviceLocalAccountPolicy();
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceLocalAccountPolicyBroker* broker =
      connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
          user_id_1_);
  ASSERT_TRUE(broker);
  broker->core()->store()->Load();

  // The external data should be fetched and cached automatically. Wait for this
  // fetch.
  run_loop->Run();

  // Stop serving external data at |kExternalDataURL|.
  fetcher_factory.reset();

  const PolicyMap::Entry* policy_entry =
      broker->core()->store()->policy_map().Get(key::kUserAvatarImage);
  ASSERT_TRUE(policy_entry);
  ASSERT_TRUE(policy_entry->external_data_fetcher);

  // Retrieve the external data. Although the data is no longer being served at
  // |kExternalDataURL|, the retrieval should succeed because the data has been
  // cached.
  run_loop.reset(new base::RunLoop);
  scoped_ptr<std::string> fetched_external_data;
  policy_entry->external_data_fetcher->Fetch(base::Bind(
      &test::ExternalDataFetchCallback,
      &fetched_external_data,
      run_loop->QuitClosure()));
  run_loop->Run();

  ASSERT_TRUE(fetched_external_data);
  EXPECT_EQ(kExternalData, *fetched_external_data);

  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  // Verify that the external data reference has propagated to the device-local
  // account's ProfilePolicyConnector.
  ProfilePolicyConnector* policy_connector =
      ProfilePolicyConnectorFactory::GetForProfile(GetProfileForTest());
  ASSERT_TRUE(policy_connector);
  const PolicyMap& policies = policy_connector->policy_service()->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_entry = policies.Get(key::kUserAvatarImage);
  ASSERT_TRUE(policy_entry);
  EXPECT_TRUE(base::Value::Equals(metadata.get(), policy_entry->value));
  ASSERT_TRUE(policy_entry->external_data_fetcher);

  // Retrieve the external data via the ProfilePolicyConnector. The retrieval
  // should succeed because the data has been cached.
  run_loop.reset(new base::RunLoop);
  fetched_external_data.reset();
  policy_entry->external_data_fetcher->Fetch(base::Bind(
      &test::ExternalDataFetchCallback,
      &fetched_external_data,
      run_loop->QuitClosure()));
  run_loop->Run();

  ASSERT_TRUE(fetched_external_data);
  EXPECT_EQ(kExternalData, *fetched_external_data);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, UserAvatarImage) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  UploadDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  std::string image_data;
  ASSERT_TRUE(base::ReadFileToString(
      test_dir.Append(chromeos::test::kUserAvatarImage1RelativePath),
      &image_data));

  std::string policy;
  base::JSONWriter::Write(test::ConstructExternalDataReference(
      embedded_test_server()->GetURL(std::string("/") +
          chromeos::test::kUserAvatarImage1RelativePath).spec(),
      image_data).get(),
      &policy);
  device_local_account_policy_.payload().mutable_useravatarimage()->set_value(
      policy);
  UploadAndInstallDeviceLocalAccountPolicy();
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceLocalAccountPolicyBroker* broker =
      connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
          user_id_1_);
  ASSERT_TRUE(broker);

  run_loop_.reset(new base::RunLoop);
  chromeos::UserManager::Get()->AddObserver(this);
  broker->core()->store()->Load();
  run_loop_->Run();
  chromeos::UserManager::Get()->RemoveObserver(this);

  scoped_ptr<gfx::ImageSkia> policy_image = chromeos::test::ImageLoader(
      test_dir.Append(chromeos::test::kUserAvatarImage1RelativePath)).Load();
  ASSERT_TRUE(policy_image);

  const user_manager::User* user =
      chromeos::UserManager::Get()->FindUser(user_id_1_);
  ASSERT_TRUE(user);

  base::FilePath user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  const base::FilePath saved_image_path =
      user_data_dir.Append(user_id_1_).AddExtension("jpg");

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, user->image_index());
  EXPECT_TRUE(chromeos::test::AreImagesEqual(*policy_image, user->GetImage()));
  const base::DictionaryValue* images_pref =
      g_browser_process->local_state()->GetDictionary("user_image_info");
  ASSERT_TRUE(images_pref);
  const base::DictionaryValue* image_properties;
  ASSERT_TRUE(images_pref->GetDictionaryWithoutPathExpansion(
      user_id_1_,
      &image_properties));
  int image_index;
  std::string image_path;
  ASSERT_TRUE(image_properties->GetInteger("index", &image_index));
  ASSERT_TRUE(image_properties->GetString("path", &image_path));
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, image_index);
  EXPECT_EQ(saved_image_path.value(), image_path);

  scoped_ptr<gfx::ImageSkia> saved_image =
      chromeos::test::ImageLoader(saved_image_path).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image->width(), saved_image->width());
  EXPECT_EQ(policy_image->height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest,
                       DISABLED_LastWindowClosedLogoutReminder) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  apps::AppWindowRegistry* app_window_registry =
      apps::AppWindowRegistry::Get(profile);
  app_window_registry->AddObserver(this);

  // Verify that the logout confirmation dialog is not showing.
  ash::LogoutConfirmationController* logout_confirmation_controller =
      ash::Shell::GetInstance()->logout_confirmation_controller();
  ASSERT_TRUE(logout_confirmation_controller);
  EXPECT_FALSE(logout_confirmation_controller->dialog_for_testing());

  // Remove policy that allows only explicitly whitelisted apps to be installed
  // in a public session.
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ASSERT_TRUE(extension_system);
  extension_system->management_policy()->UnregisterAllProviders();

  // Install and a platform app.
  scoped_refptr<extensions::CrxInstaller> installer =
      extensions::CrxInstaller::CreateSilent(
          extension_system->extension_service());
  installer->set_allow_silent_install(true);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);
  installer->set_creation_flags(extensions::Extension::FROM_WEBSTORE);
  content::WindowedNotificationObserver app_install_observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  installer->InstallCrx(test_dir.Append(kPackagedAppCRXPath));
  app_install_observer.Wait();
  const extensions::Extension* app =
      content::Details<const extensions::Extension>(
          app_install_observer.details()).ptr();

  // Start the platform app, causing it to open a window.
  run_loop_.reset(new base::RunLoop);
  OpenApplication(AppLaunchParams(
      profile, app, extensions::LAUNCH_CONTAINER_NONE, NEW_WINDOW));
  run_loop_->Run();
  EXPECT_EQ(1U, app_window_registry->app_windows().size());

  // Close the only open browser window.
  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  BrowserWindow* browser_window = browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_.reset(new base::RunLoop);
  browser_window->Close();
  browser_window = NULL;
  run_loop_->Run();
  browser = NULL;
  EXPECT_TRUE(browser_list->empty());

  // Verify that the logout confirmation dialog is not showing because an app
  // window is still open.
  EXPECT_FALSE(logout_confirmation_controller->dialog_for_testing());

  // Open a browser window.
  Browser* first_browser = CreateBrowser(profile);
  EXPECT_EQ(1U, browser_list->size());

  // Close the app window.
  run_loop_.reset(new base::RunLoop);
  ASSERT_EQ(1U, app_window_registry->app_windows().size());
  app_window_registry->app_windows().front()->GetBaseWindow()->Close();
  run_loop_->Run();
  EXPECT_TRUE(app_window_registry->app_windows().empty());

  // Verify that the logout confirmation dialog is not showing because a browser
  // window is still open.
  EXPECT_FALSE(logout_confirmation_controller->dialog_for_testing());

  // Open a second browser window.
  Browser* second_browser = CreateBrowser(profile);
  EXPECT_EQ(2U, browser_list->size());

  // Close the first browser window.
  browser_window = first_browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_.reset(new base::RunLoop);
  browser_window->Close();
  browser_window = NULL;
  run_loop_->Run();
  first_browser = NULL;
  EXPECT_EQ(1U, browser_list->size());

  // Verify that the logout confirmation dialog is not showing because a browser
  // window is still open.
  EXPECT_FALSE(logout_confirmation_controller->dialog_for_testing());

  // Close the second browser window.
  browser_window = second_browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_.reset(new base::RunLoop);
  browser_window->Close();
  browser_window = NULL;
  run_loop_->Run();
  second_browser = NULL;
  EXPECT_TRUE(browser_list->empty());

  // Verify that the logout confirmation dialog is showing.
  ash::LogoutConfirmationDialog* dialog =
      logout_confirmation_controller->dialog_for_testing();
  ASSERT_TRUE(dialog);

  // Deny the logout.
  dialog->GetWidget()->Close();
  dialog = NULL;
  base::RunLoop().RunUntilIdle();

  // Verify that the logout confirmation dialog is no longer showing.
  EXPECT_FALSE(logout_confirmation_controller->dialog_for_testing());

  // Open a browser window.
  browser = CreateBrowser(profile);
  EXPECT_EQ(1U, browser_list->size());

  // Close the browser window.
  browser_window = browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_.reset(new base::RunLoop);
  browser_window->Close();
  browser_window = NULL;
  run_loop_->Run();
  browser = NULL;
  EXPECT_TRUE(browser_list->empty());

  // Verify that the logout confirmation dialog is showing again.
  dialog = logout_confirmation_controller->dialog_for_testing();
  ASSERT_TRUE(dialog);

  // Deny the logout.
  dialog->GetWidget()->Close();
  dialog = NULL;
  base::RunLoop().RunUntilIdle();

  app_window_registry->RemoveObserver(this);
};

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest,
                       DISABLED_DoNotSelectLanguageAndKeyboard) {
  chromeos::input_method::InputMethodManager* input_method_manager =
      chromeos::input_method::InputMethodManager::Get();
  const std::string initial_locale = g_browser_process->GetApplicationLocale();
  const std::string initial_input_method =
      input_method_manager->GetCurrentInputMethod().id();

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  EXPECT_EQ(initial_locale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(initial_input_method,
            input_method_manager->GetCurrentInputMethod().id());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest,
                       DISABLED_SelectLanguageAndKeyboard) {
  // Specify startup pages.
  device_local_account_policy_.payload().mutable_restoreonstartup()->set_value(
      SessionStartupPref::kPrefValueURLs);
  em::StringListPolicyProto* startup_urls_proto =
      device_local_account_policy_.payload().mutable_restoreonstartupurls();
  for (size_t i = 0; i < arraysize(kStartupURLs); ++i)
    startup_urls_proto->mutable_value()->add_entries(kStartupURLs[i]);
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  // Log in to the device-local account with a specific locale and keyboard
  // layout.
  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());
  ASSERT_NO_FATAL_FAILURE(
      StartLogin(kPublicSessionLocale, public_session_input_method_id_));
  WaitForSessionStart();

  // Verify that the locale and keyboard layout have been applied.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(public_session_input_method_id_,
            chromeos::input_method::InputMethodManager::Get()->
                GetCurrentInputMethod().id());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest,
                       DISABLED_SelectLanguageAndKeyboardWithTermsOfService) {
  // Specify Terms of Service URL.
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  device_local_account_policy_.payload().mutable_termsofserviceurl()->set_value(
      embedded_test_server()->GetURL(
          std::string("/") + kExistentTermsOfServicePath).spec());
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  // Log in to the device-local account with a specific locale and keyboard
  // layout.
  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());
  ASSERT_NO_FATAL_FAILURE(
      StartLogin(kPublicSessionLocale, public_session_input_method_id_));

  // Set up an observer that will quit the message loop when login has succeeded
  // and the first wizard screen, if any, is being shown.
  base::RunLoop login_wait_run_loop;
  chromeos::MockAuthStatusConsumer login_status_consumer;
  EXPECT_CALL(login_status_consumer, OnAuthSuccess(_)).Times(1).WillOnce(
      InvokeWithoutArgs(&login_wait_run_loop, &base::RunLoop::Quit));

  // Spin the loop until the observer fires. Then, unregister the observer.
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
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

  // Wait for the Terms of Service to finish downloading.
  content::WebContents* contents = NULL;
  ASSERT_NO_FATAL_FAILURE(GetWebContents(&contents));
  bool done = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(contents,
      "var screenElement = document.getElementById('terms-of-service');"
      "function SendReplyIfDownloadDone() {"
      "  if (screenElement.classList.contains('tos-loading'))"
      "    return false;"
      "  domAutomationController.send(true);"
      "  observer.disconnect();"
      "  return true;"
      "}"
      "var observer = new MutationObserver(SendReplyIfDownloadDone);"
      "if (!SendReplyIfDownloadDone()) {"
      "  var options = { attributes: true, attributeFilter: [ 'class' ] };"
      "  observer.observe(screenElement, options);"
      "}",
      &done));

  // Verify that the locale and keyboard layout have been applied.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(public_session_input_method_id_,
            chromeos::input_method::InputMethodManager::Get()->
                GetCurrentInputMethod().id());

  // Click the accept button.
  ASSERT_TRUE(content::ExecuteScript(contents,
                                     "$('tos-accept-button').click();"));

  WaitForSessionStart();

  // Verify that the locale and keyboard layout are still in force.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(public_session_input_method_id_,
            chromeos::input_method::InputMethodManager::Get()->
                GetCurrentInputMethod().id());
}

class TermsOfServiceDownloadTest : public DeviceLocalAccountTest,
                                   public testing::WithParamInterface<bool> {
};

IN_PROC_BROWSER_TEST_P(TermsOfServiceDownloadTest,
                       DISABLED_TermsOfServiceScreen) {
  // Specify Terms of Service URL.
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  device_local_account_policy_.payload().mutable_termsofserviceurl()->set_value(
      embedded_test_server()->GetURL(
            std::string("/") +
                (GetParam() ? kExistentTermsOfServicePath
                            : kNonexistentTermsOfServicePath)).spec());
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(WaitForLoginUI());
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Set up an observer that will quit the message loop when login has succeeded
  // and the first wizard screen, if any, is being shown.
  base::RunLoop login_wait_run_loop;
  chromeos::MockAuthStatusConsumer login_status_consumer;
  EXPECT_CALL(login_status_consumer, OnAuthSuccess(_)).Times(1).WillOnce(
      InvokeWithoutArgs(&login_wait_run_loop, &base::RunLoop::Quit));

  // Spin the loop until the observer fires. Then, unregister the observer.
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
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
  content::WebContents* contents = NULL;
  ASSERT_NO_FATAL_FAILURE(GetWebContents(&contents));
  std::string json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents,
      "var screenElement = document.getElementById('terms-of-service');"
      "function SendReplyIfDownloadDone() {"
      "  if (screenElement.classList.contains('tos-loading'))"
      "    return false;"
      "  var status = {};"
      "  status.heading = document.getElementById('tos-heading').textContent;"
      "  status.subheading ="
      "      document.getElementById('tos-subheading').textContent;"
      "  status.contentHeading ="
      "      document.getElementById('tos-content-heading').textContent;"
      "  status.content ="
      "      document.getElementById('tos-content-main').textContent;"
      "  status.error = screenElement.classList.contains('error');"
      "  status.acceptEnabled ="
      "      !document.getElementById('tos-accept-button').disabled;"
      "  domAutomationController.send(JSON.stringify(status));"
      "  observer.disconnect();"
      "  return true;"
      "}"
      "var observer = new MutationObserver(SendReplyIfDownloadDone);"
      "if (!SendReplyIfDownloadDone()) {"
      "  var options = { attributes: true, attributeFilter: [ 'class' ] };"
      "  observer.observe(screenElement, options);"
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
                                base::UTF8ToUTF16(kDomain)),
      heading);
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_TERMS_OF_SERVICE_SCREEN_SUBHEADING,
                                base::UTF8ToUTF16(kDomain)),
      subheading);
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_TERMS_OF_SERVICE_SCREEN_CONTENT_HEADING,
                                base::UTF8ToUTF16(kDomain)),
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

  WaitForSessionStart();
}

INSTANTIATE_TEST_CASE_P(TermsOfServiceDownloadTestInstance,
                        TermsOfServiceDownloadTest, testing::Bool());

}  // namespace policy
