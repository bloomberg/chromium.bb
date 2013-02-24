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
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/install_attributes.pb.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace em = enterprise_management;

using testing::Return;

namespace policy {

namespace {

const char kAccountId1[] = "dla1@example.com";
const char kAccountId2[] = "dla2@example.com";
const char kDisplayName1[] = "display name for account 1";
const char kDisplayName2[] = "display name for account 2";
const char* kStartupURLs[] = {
  "chrome://policy",
  "chrome://about",
};

// Observes a specific notification type and quits the message loop once a
// condition holds.
class NotificationWatcher : public content::NotificationObserver {
 public:
  // Callback invoked on notifications. Should return true when the condition
  // that the caller is waiting for is satisfied.
  typedef base::Callback<bool(void)> ConditionTestCallback;

  explicit NotificationWatcher(int notification_type,
                               const ConditionTestCallback& callback)
      : type_(notification_type),
        callback_(callback) {}

  void Run() {
    if (callback_.Run())
      return;

    content::NotificationRegistrar registrar;
    registrar.Add(this, type_, content::NotificationService::AllSources());
    run_loop_.Run();
  }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (callback_.Run())
      run_loop_.Quit();
  }

 private:
  int type_;
  ConditionTestCallback callback_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(NotificationWatcher);
};

// A fake implementation of session_manager. Accepts policy blobs to be set and
// returns them unmodified.
class FakeSessionManagerClient : public chromeos::SessionManagerClient {
 public:
  FakeSessionManagerClient() {}
  virtual ~FakeSessionManagerClient() {}

  // SessionManagerClient:
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual bool HasObserver(Observer* observer) OVERRIDE { return false; }
  virtual void EmitLoginPromptReady() OVERRIDE {}
  virtual void EmitLoginPromptVisible() OVERRIDE {}
  virtual void RestartJob(int pid, const std::string& command_line) OVERRIDE {}
  virtual void RestartEntd() OVERRIDE {}
  virtual void StartSession(const std::string& user_email) OVERRIDE {}
  virtual void StopSession() OVERRIDE {}
  virtual void StartDeviceWipe() OVERRIDE {}
  virtual void RequestLockScreen() OVERRIDE {}
  virtual void NotifyLockScreenShown() OVERRIDE {}
  virtual void RequestUnlockScreen() OVERRIDE {}
  virtual void NotifyLockScreenDismissed() OVERRIDE {}
  virtual void RetrieveDevicePolicy(
      const RetrievePolicyCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback, device_policy_));
  }
  virtual void RetrieveUserPolicy(
      const RetrievePolicyCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback, user_policy_));
  }
  virtual void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      const RetrievePolicyCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, device_local_account_policy_[account_id]));
  }
  virtual void StoreDevicePolicy(const std::string& policy_blob,
                                 const StorePolicyCallback& callback) OVERRIDE {
    device_policy_ = policy_blob;
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, true));
  }
  virtual void StoreUserPolicy(const std::string& policy_blob,
                               const StorePolicyCallback& callback) OVERRIDE {
    user_policy_ = policy_blob;
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, true));
  }
  virtual void StoreDeviceLocalAccountPolicy(
      const std::string& account_id,
      const std::string& policy_blob,
      const StorePolicyCallback& callback) OVERRIDE {
    device_local_account_policy_[account_id] = policy_blob;
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, true));
  }

  const std::string& device_policy() const {
    return device_policy_;
  }
  void set_device_policy(const std::string& policy_blob) {
    device_policy_ = policy_blob;
  }

  const std::string& user_policy() const {
    return user_policy_;
  }
  void set_user_policy(const std::string& policy_blob) {
    user_policy_ = policy_blob;
  }

  const std::string& device_local_account_policy(
      const std::string& account_id) const {
    std::map<std::string, std::string>::const_iterator entry =
        device_local_account_policy_.find(account_id);
    return entry != device_local_account_policy_.end() ? entry->second
                                                       : EmptyString();
  }
  void set_device_local_account_policy(const std::string& account_id,
                                       const std::string& policy_blob) {
    device_local_account_policy_[account_id] = policy_blob;
  }

 private:
  std::string device_policy_;
  std::string user_policy_;
  std::map<std::string, std::string> device_local_account_policy_;

  DISALLOW_COPY_AND_ASSIGN(FakeSessionManagerClient);
};

class FakeCryptohomeClient : public chromeos::CryptohomeClient {
 public:
  using chromeos::CryptohomeClient::AsyncMethodCallback;
  using chromeos::CryptohomeClient::AsyncCallStatusHandler;
  using chromeos::CryptohomeClient::AsyncCallStatusWithDataHandler;

  FakeCryptohomeClient() {}
  virtual ~FakeCryptohomeClient() {}

  virtual void SetAsyncCallStatusHandlers(
      const AsyncCallStatusHandler& handler,
      const AsyncCallStatusWithDataHandler& data_handler) OVERRIDE {
    handler_ = handler;
    data_handler_ = data_handler;
  }
  virtual void ResetAsyncCallStatusHandlers() OVERRIDE {
    handler_.Reset();
    data_handler_.Reset();
  }
  virtual void IsMounted(
      const chromeos::BoolDBusMethodCallback& callback) OVERRIDE {}
  virtual bool Unmount(bool* success) OVERRIDE {
    *success = true;
    return true;
  }
  virtual void AsyncCheckKey(const std::string& username,
                             const std::string& key,
                             const AsyncMethodCallback& callback) OVERRIDE {}
  virtual void AsyncMigrateKey(const std::string& username,
                               const std::string& from_key,
                               const std::string& to_key,
                               const AsyncMethodCallback& callback) OVERRIDE {}
  virtual void AsyncRemove(const std::string& username,
                           const AsyncMethodCallback& callback) OVERRIDE {}
  virtual bool GetSystemSalt(std::vector<uint8>* salt) OVERRIDE {
    return false;
  }
  virtual void GetSanitizedUsername(
      const std::string& username,
      const chromeos::StringDBusMethodCallback& callback) OVERRIDE {}
  virtual void AsyncMount(const std::string& username,
                          const std::string& key,
                          int flags,
                          const AsyncMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, 1));
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(handler_, 1, true, cryptohome::MOUNT_ERROR_NONE));
  }
  virtual void AsyncMountGuest(const AsyncMethodCallback& callback) OVERRIDE {}
  virtual void TpmIsReady(
      const chromeos::BoolDBusMethodCallback& callback) OVERRIDE {}
  virtual void TpmIsEnabled(
      const chromeos::BoolDBusMethodCallback& callback) OVERRIDE {}
  virtual bool CallTpmIsEnabledAndBlock(bool* enabled) OVERRIDE {
    return false;
  }
  virtual void TpmGetPassword(
      const chromeos::StringDBusMethodCallback& callback) OVERRIDE {}
  virtual void TpmIsOwned(
      const chromeos::BoolDBusMethodCallback& kallback) OVERRIDE {}
  virtual bool CallTpmIsOwnedAndBlock(bool* owned) OVERRIDE {
    return false;
  }
  virtual void TpmIsBeingOwned(
      const chromeos::BoolDBusMethodCallback& kallback) OVERRIDE {}
  virtual bool CallTpmIsBeingOwnedAndBlock(bool* owning) OVERRIDE {
    return false;
  }
  virtual void TpmCanAttemptOwnership(
      const chromeos::VoidDBusMethodCallback& callback) OVERRIDE {}
  virtual bool CallTpmClearStoredPasswordAndBlock() OVERRIDE {
    return false;
  }
  virtual void TpmClearStoredPassword(
      const chromeos::VoidDBusMethodCallback& kallback) OVERRIDE {}
  virtual void Pkcs11IsTpmTokenReady(
      const chromeos::BoolDBusMethodCallback& callback) OVERRIDE {}
  virtual void Pkcs11GetTpmTokenInfo(
      const Pkcs11GetTpmTokenInfoCallback& callback) OVERRIDE {}
  virtual bool InstallAttributesGet(const std::string& name,
                                    std::vector<uint8>* value,
                                    bool* successful) OVERRIDE {
    return false;
  }
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::vector<uint8>& value,
                                    bool* successful) OVERRIDE {
    return false;
  }
  virtual bool InstallAttributesFinalize(bool* successful) OVERRIDE {
    return false;
  }
  virtual bool InstallAttributesIsReady(bool* is_ready) OVERRIDE {
    return false;
  }
  virtual bool InstallAttributesIsInvalid(bool* is_invalid) OVERRIDE {
    return true;
  }
  virtual bool InstallAttributesIsFirstInstall(
      bool* is_first_install) OVERRIDE {
    return false;
  }
  virtual void TpmAttestationIsPrepared(
        const chromeos::BoolDBusMethodCallback& callback) OVERRIDE {}
  virtual void TpmAttestationIsEnrolled(
        const chromeos::BoolDBusMethodCallback& callback) OVERRIDE {}
  virtual void AsyncTpmAttestationCreateEnrollRequest(
      const AsyncMethodCallback& callback) OVERRIDE {}
  virtual void AsyncTpmAttestationEnroll(
      const std::string& pca_response,
      const AsyncMethodCallback& callback) OVERRIDE {}
  virtual void AsyncTpmAttestationCreateCertRequest(
      bool is_cert_for_owner,
      const AsyncMethodCallback& callback) OVERRIDE {}
  virtual void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      const AsyncMethodCallback& callback) OVERRIDE {}

 private:
  AsyncCallStatusHandler handler_;
  AsyncCallStatusWithDataHandler data_handler_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptohomeClient);
};

}  // namespace

class DeviceLocalAccountTest : public InProcessBrowserTest {
 protected:
  DeviceLocalAccountTest() {}
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

    InProcessBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(
        switches::kLoginScreen, chromeos::WizardController::kLoginScreenName);
    command_line->AppendSwitchASCII(
        switches::kDeviceManagementUrl, test_server_.GetServiceURL().spec());
    command_line->AppendSwitchASCII(
        switches::kLoginProfile, "user");
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Clear command-line arguments (but keep command-line switches) so the
    // startup pages policy takes effect.
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    CommandLine::StringVector argv(command_line->argv());
    argv.erase(argv.begin() + argv.size() - command_line->GetArgs().size(),
               argv.end());
    command_line->InitFromArgv(argv);

    // Mark the device enterprise-enrolled.
    SetUpInstallAttributes();

    SetUpPolicy();

    // Redirect session_manager DBus calls to FakeSessionManagerClient.
    chromeos::MockDBusThreadManager* dbus_thread_manager =
        new chromeos::MockDBusThreadManager();
    EXPECT_CALL(*dbus_thread_manager, GetSessionManagerClient())
        .WillRepeatedly(Return(&session_manager_client_));
    chromeos::DBusThreadManager::InitializeForTesting(dbus_thread_manager);

    // Mock out cryptohome mount calls to succeed immediately.
    EXPECT_CALL(*dbus_thread_manager, GetCryptohomeClient())
        .WillRepeatedly(Return(&cryptohome_client_));
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // This shuts down the login UI.
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(&chrome::AttemptExit));
    base::RunLoop().RunUntilIdle();
  }

  void SetUpInstallAttributes() {
    cryptohome::SerializedInstallAttributes install_attrs_proto;
    cryptohome::SerializedInstallAttributes::Attribute* attribute = NULL;

    attribute = install_attrs_proto.add_attributes();
    attribute->set_name(EnterpriseInstallAttributes::kAttrEnterpriseOwned);
    attribute->set_value("true");

    attribute = install_attrs_proto.add_attributes();
    attribute->set_name(EnterpriseInstallAttributes::kAttrEnterpriseUser);
    attribute->set_value(PolicyBuilder::kFakeUsername);

    base::FilePath install_attrs_file =
        temp_dir_.path().AppendASCII("install_attributes.pb");
    const std::string install_attrs_blob(
        install_attrs_proto.SerializeAsString());
    ASSERT_EQ(static_cast<int>(install_attrs_blob.size()),
              file_util::WriteFile(install_attrs_file,
                                   install_attrs_blob.c_str(),
                                   install_attrs_blob.size()));
    ASSERT_TRUE(PathService::Override(chrome::FILE_INSTALL_ATTRIBUTES,
                                      install_attrs_file));
  }

  void SetUpPolicy() {
    // Configure two device-local accounts in device settings.
    DevicePolicyBuilder device_policy;
    device_policy.policy_data().set_public_key_version(1);
    em::ChromeDeviceSettingsProto& proto(device_policy.payload());
    proto.mutable_show_user_names()->set_show_user_names(true);
    proto.mutable_device_local_accounts()->add_account()->set_id(kAccountId1);
    proto.mutable_device_local_accounts()->add_account()->set_id(kAccountId2);
    device_policy.Build();
    session_manager_client_.set_device_policy(device_policy.GetBlob());
    test_server_.UpdatePolicy(dm_protocol::kChromeDevicePolicyType,
                              std::string(), proto.SerializeAsString());

    // Install the owner key.
    base::FilePath owner_key_file = temp_dir_.path().AppendASCII("owner.key");
    std::vector<uint8> owner_key_bits;
    ASSERT_TRUE(device_policy.signing_key()->ExportPublicKey(&owner_key_bits));
    ASSERT_EQ(
        static_cast<int>(owner_key_bits.size()),
        file_util::WriteFile(
            owner_key_file,
            reinterpret_cast<const char*>(vector_as_array(&owner_key_bits)),
            owner_key_bits.size()));
    ASSERT_TRUE(PathService::Override(chrome::FILE_OWNER_KEY, owner_key_file));

    // Configure device-local account policy for the first device-local account.
    UserPolicyBuilder device_local_account_policy;
    device_local_account_policy.policy_data().set_policy_type(
        dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy.policy_data().set_username(kAccountId1);
    device_local_account_policy.policy_data().set_settings_entity_id(
        kAccountId1);
    device_local_account_policy.policy_data().set_public_key_version(1);
    device_local_account_policy.payload().mutable_restoreonstartup()->set_value(
        SessionStartupPref::kPrefValueURLs);
    em::StringListPolicyProto* startup_urls_proto =
        device_local_account_policy.payload().mutable_restoreonstartupurls();
    for (size_t i = 0; i < arraysize(kStartupURLs); ++i)
      startup_urls_proto->mutable_value()->add_entries(kStartupURLs[i]);
    device_local_account_policy.payload().mutable_userdisplayname()->set_value(
        kDisplayName1);
    device_local_account_policy.Build();
    session_manager_client_.set_device_local_account_policy(
        kAccountId1, device_local_account_policy.GetBlob());
    test_server_.UpdatePolicy(
        dm_protocol::kChromePublicAccountPolicyType, kAccountId1,
        device_local_account_policy.payload().SerializeAsString());

    // Make policy for the second account available from the server.
    device_local_account_policy.payload().mutable_userdisplayname()->set_value(
        kDisplayName2);
    test_server_.UpdatePolicy(
        dm_protocol::kChromePublicAccountPolicyType, kAccountId2,
        device_local_account_policy.payload().SerializeAsString());
  }

  void CheckPublicSessionPresent(const std::string& id) {
    const chromeos::User* user = chromeos::UserManager::Get()->FindUser(id);
    ASSERT_TRUE(user);
    EXPECT_EQ(id, user->email());
    EXPECT_EQ(chromeos::User::USER_TYPE_PUBLIC_ACCOUNT, user->GetType());
  }

  LocalPolicyTestServer test_server_;
  base::ScopedTempDir temp_dir_;

  FakeSessionManagerClient session_manager_client_;
  FakeCryptohomeClient cryptohome_client_;
};

static bool IsKnownUser(const std::string& account_id) {
  return chromeos::UserManager::Get()->IsKnownUser(account_id);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, LoginScreen) {
  NotificationWatcher(chrome::NOTIFICATION_USER_LIST_CHANGED,
                      base::Bind(&IsKnownUser, kAccountId1)).Run();
  NotificationWatcher(chrome::NOTIFICATION_USER_LIST_CHANGED,
                      base::Bind(&IsKnownUser, kAccountId2)).Run();

  CheckPublicSessionPresent(kAccountId1);
  CheckPublicSessionPresent(kAccountId2);
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
  NotificationWatcher(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, kAccountId1, kDisplayName1)).Run();
}

#if defined(OS_CHROMEOS)
// Fails on ChromeOS http://crbug.com/177880
#define MAYBE_PolicyDownload DISABLED_PolicyDownload
#else
#define MAYBE_PolicyDownload PolicyDownload
#endif
IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, MAYBE_PolicyDownload) {
  // Policy for kAccountId2 is not installed in session_manager_client, make
  // sure it gets fetched from the server. Note that the test setup doesn't set
  // up policy for kAccountId2, so the presence of the display name can be used
  // as signal to indicate successful policy download.
  ASSERT_TRUE(session_manager_client_.device_local_account_policy(
      kAccountId2).empty());
  NotificationWatcher(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, kAccountId2, kDisplayName2)).Run();

  // Sanity check: The policy should be present now.
  ASSERT_FALSE(session_manager_client_.device_local_account_policy(
      kAccountId2).empty());
}

static bool IsNotKnownUser(const std::string& account_id) {
  return !IsKnownUser(account_id);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DevicePolicyChange) {
  // Wait until the login screen is up.
  NotificationWatcher(chrome::NOTIFICATION_USER_LIST_CHANGED,
                      base::Bind(&IsKnownUser, kAccountId1)).Run();
  NotificationWatcher(chrome::NOTIFICATION_USER_LIST_CHANGED,
                      base::Bind(&IsKnownUser, kAccountId2)).Run();

  // Update policy to remove kAccountId2.
  em::ChromeDeviceSettingsProto policy;
  policy.mutable_show_user_names()->set_show_user_names(true);
  policy.mutable_device_local_accounts()->add_account()->set_id(kAccountId1);

  test_server_.UpdatePolicy(dm_protocol::kChromeDevicePolicyType, std::string(),
                            policy.SerializeAsString());
  g_browser_process->policy_service()->RefreshPolicies(base::Closure());

  // Make sure the second device-local account disappears.
  NotificationWatcher(chrome::NOTIFICATION_USER_LIST_CHANGED,
                      base::Bind(&IsNotKnownUser, kAccountId2)).Run();
}

static bool IsSessionStarted() {
  return chromeos::UserManager::Get()->IsSessionStarted();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, StartSession) {
  // This observes the display name becoming available as this indicates
  // device-local account policy is fully loaded, which is a prerequisite for
  // successful login.
  NotificationWatcher(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, kAccountId1, kDisplayName1)).Run();

  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
  controller->LoginAsPublicAccount(kAccountId1);

  // Wait for the session to start.
  NotificationWatcher(chrome::NOTIFICATION_SESSION_STARTED,
                      base::Bind(IsSessionStarted)).Run();

  // Check that the startup pages specified in policy were opened.
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  Browser* browser =
      chrome::FindLastActiveWithHostDesktopType(chrome::HOST_DESKTOP_TYPE_ASH);
  ASSERT_TRUE(browser);

  TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  int expected_tab_count = static_cast<int>(arraysize(kStartupURLs));
  EXPECT_EQ(expected_tab_count, tabs->count());
  for (int i = 0; i < expected_tab_count && i < tabs->count(); ++i)
    EXPECT_EQ(GURL(kStartupURLs[i]), tabs->GetWebContentsAt(i)->GetURL());
}

}  // namespace policy
