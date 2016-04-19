// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/app_mode/fake_cws.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/test/app_window_waiter.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

const char kTestKioskApp[] = "ggbflgnkafappblpkiflbgpmkfdpnhhe";

void CreateAndInitializeLocalCache() {
  base::FilePath extension_cache_dir;
  CHECK(PathService::Get(chromeos::DIR_DEVICE_EXTENSION_LOCAL_CACHE,
                         &extension_cache_dir));
  base::FilePath cache_init_file = extension_cache_dir.Append(
      extensions::LocalExtensionCache::kCacheReadyFlagFileName);
  EXPECT_EQ(base::WriteFile(cache_init_file, "", 0), 0);
}

}  // namespace

class KioskCrashRestoreTest : public InProcessBrowserTest {
 public:
  KioskCrashRestoreTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()),
        fake_cws_(new FakeCWS) {}

  // InProcessBrowserTest
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  bool SetUpUserDataDirectory() override {
    SetUpExistingKioskApp();
    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SimulateNetworkOnline();

    OverrideDevicePolicy();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const AccountId account_id = AccountId::FromUserEmail(GetTestAppUserId());
    const cryptohome::Identification cryptohome_id(account_id);

    command_line->AppendSwitchASCII(switches::kLoginUser, cryptohome_id.id());
    command_line->AppendSwitchASCII(
        switches::kLoginProfile,
        CryptohomeClient::GetStubSanitizedUsername(cryptohome_id));

    fake_cws_->Init(embedded_test_server());
    fake_cws_->SetUpdateCrx(test_app_id_, test_app_id_ + ".crx", "1.0.0");
  }

  void SetUpOnMainThread() override {
    CreateAndInitializeLocalCache();

    embedded_test_server()->StartAcceptingConnections();
  }

  const std::string GetTestAppUserId() const {
    return policy::GenerateDeviceLocalAccountUserId(
        test_app_id_, policy::DeviceLocalAccount::TYPE_KIOSK_APP);
  }

  const std::string& test_app_id() const { return test_app_id_; }

 private:
  void SetUpExistingKioskApp() {
    // Create policy data that contains the test app as an existing kiosk app.
    em::DeviceLocalAccountsProto* const device_local_accounts =
        device_policy_.payload().mutable_device_local_accounts();

    em::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(test_app_id_);
    account->set_type(
        em::DeviceLocalAccountInfoProto_AccountType_ACCOUNT_TYPE_KIOSK_APP);
    account->mutable_kiosk_app()->set_app_id(test_app_id_);
    device_policy_.Build();

    // Prepare the policy data to store in device policy cache.
    em::PolicyData policy_data;
    CHECK(device_policy_.payload().SerializeToString(
        policy_data.mutable_policy_value()));
    const std::string policy_data_string = policy_data.SerializeAsString();
    std::string encoded;
    base::Base64Encode(policy_data_string, &encoded);

    // Store policy data and existing device local accounts in local state.
    const std::string local_state_json =
        extensions::DictionaryBuilder()
            .Set(prefs::kDeviceSettingsCache, encoded)
            .Set("PublicAccounts",
                 extensions::ListBuilder().Append(GetTestAppUserId()).Build())
            .ToJSON();

    base::FilePath local_state_file;
    CHECK(PathService::Get(chrome::DIR_USER_DATA, &local_state_file));
    local_state_file = local_state_file.Append(chrome::kLocalStateFilename);
    base::WriteFile(local_state_file, local_state_json.data(),
                    local_state_json.size());
  }

  void SimulateNetworkOnline() {
    NetworkPortalDetectorTestImpl* const network_portal_detector =
        new NetworkPortalDetectorTestImpl();
    // Takes ownership of |network_portal_detector|.
    network_portal_detector::InitializeForTesting(network_portal_detector);
    network_portal_detector->SetDefaultNetworkForTesting(
        FakeShillManagerClient::kFakeEthernetNetworkGuid);

    NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;
    network_portal_detector->SetDetectionResultsForTesting(
        FakeShillManagerClient::kFakeEthernetNetworkGuid, online_state);
  }

  void OverrideDevicePolicy() {
    OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());

    session_manager_client_ = new FakeSessionManagerClient;
    session_manager_client_->set_device_policy(device_policy_.GetBlob());

    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(session_manager_client_));
  }

  std::string test_app_id_ = kTestKioskApp;

  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  FakeSessionManagerClient* session_manager_client_;
  std::unique_ptr<FakeCWS> fake_cws_;

  DISALLOW_COPY_AND_ASSIGN(KioskCrashRestoreTest);
};

IN_PROC_BROWSER_TEST_F(KioskCrashRestoreTest, Basic) {
  ExtensionTestMessageListener launch_data_check_listener(
      "launchData.isKioskSession = true", false);

  Profile* const app_profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(app_profile);
  extensions::AppWindowRegistry* const app_window_registry =
      extensions::AppWindowRegistry::Get(app_profile);
  extensions::AppWindow* const window =
      AppWindowWaiter(app_window_registry, test_app_id()).Wait();
  ASSERT_TRUE(window);

  window->GetBaseWindow()->Close();

  // Wait until the app terminates if it is still running.
  if (!app_window_registry->GetAppWindowsForApp(test_app_id()).empty())
    base::RunLoop().Run();

  EXPECT_TRUE(launch_data_check_listener.was_satisfied());
}

}  // namespace chromeos
