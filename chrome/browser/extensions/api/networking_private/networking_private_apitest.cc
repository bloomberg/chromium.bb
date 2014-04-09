// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "policy/policy_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#else  // !defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client_factory.h"
#include "components/wifi/wifi_service.h"
#endif  // defined(OS_CHROMEOS)

using testing::Return;
using testing::_;

#if defined(OS_CHROMEOS)
using chromeos::CryptohomeClient;
using chromeos::DBUS_METHOD_CALL_SUCCESS;
using chromeos::DBusMethodCallStatus;
using chromeos::DBusThreadManager;
using chromeos::NetworkPortalDetector;
using chromeos::NetworkPortalDetectorTestImpl;
using chromeos::ShillDeviceClient;
using chromeos::ShillManagerClient;
using chromeos::ShillProfileClient;
using chromeos::ShillServiceClient;
#else  // !defined(OS_CHROMEOS)
using extensions::NetworkingPrivateServiceClientFactory;
#endif  // defined(OS_CHROMEOS)

namespace {

#if defined(OS_CHROMEOS)
const char kUser1ProfilePath[] = "/profile/user1/shill";
#else  // !defined(OS_CHROMEOS)

// Stub Verify* methods implementation to satisfy expectations of
// networking_private_apitest.
// TODO(mef): Fix ChromeOS implementation to use NetworkingPrivateCrypto,
// and update networking_private_apitest to use and expect valid data.
// That will eliminate the need for mock implementation.
class CryptoVerifyStub
    : public extensions::NetworkingPrivateServiceClient::CryptoVerify {
  virtual void VerifyDestination(scoped_ptr<base::ListValue> args,
                                 bool* verified,
                                 std::string* error) OVERRIDE {
    *verified = true;
  }

  virtual void VerifyAndEncryptCredentials(
      scoped_ptr<base::ListValue> args,
      const extensions::NetworkingPrivateServiceClient::CryptoVerify::
          VerifyAndEncryptCredentialsCallback& callback) OVERRIDE {
    callback.Run("encrypted_credentials", "");
  }

  virtual void VerifyAndEncryptData(scoped_ptr<base::ListValue> args,
                                    std::string* encoded_data,
                                    std::string* error) OVERRIDE {
    *encoded_data = "encrypted_data";
  }
};
#endif  // defined(OS_CHROMEOS)

class ExtensionNetworkingPrivateApiTest :
    public ExtensionApiTest,
    public testing::WithParamInterface<bool> {
 public:
  bool RunNetworkingSubtest(const std::string& subtest) {
    return RunExtensionSubtest(
        "networking", "main.html?" + subtest,
        kFlagEnableFileAccess | kFlagLoadAsComponent);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
#if defined(OS_CHROMEOS)
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
#endif

    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

#if defined(OS_CHROMEOS)
  static void AssignString(std::string* out,
                    DBusMethodCallStatus call_status,
                    const std::string& result) {
    CHECK_EQ(call_status, DBUS_METHOD_CALL_SUCCESS);
    *out = result;
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Whitelist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");

    // TODO(pneubeck): Remove the following hack, once the NetworkingPrivateAPI
    // uses the ProfileHelper to obtain the userhash crbug/238623.
    std::string login_user =
        command_line->GetSwitchValueNative(chromeos::switches::kLoginUser);
    std::string sanitized_user = CryptohomeClient::GetStubSanitizedUsername(
        login_user);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    sanitized_user);
    if (GetParam())
      command_line->AppendSwitch(::switches::kMultiProfiles);
  }

  void InitializeSanitizedUsername() {
    chromeos::UserManager* user_manager = chromeos::UserManager::Get();
    chromeos::User* user = user_manager->GetActiveUser();
    CHECK(user);
    std::string userhash;
    DBusThreadManager::Get()->GetCryptohomeClient()->GetSanitizedUsername(
        user->email(),
        base::Bind(&AssignString, &userhash_));
    content::RunAllPendingInMessageLoop();
    CHECK(!userhash_.empty());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    InitializeSanitizedUsername();

    ShillManagerClient::TestInterface* manager_test =
        DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
    ShillDeviceClient::TestInterface* device_test =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    ShillProfileClient::TestInterface* profile_test =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();

    device_test->ClearDevices();
    service_test->ClearServices();

    // Sends a notification about the added profile.
    profile_test->AddProfile(kUser1ProfilePath, userhash_);

    device_test->AddDevice("/device/stub_wifi_device1",
                           shill::kTypeWifi, "stub_wifi_device1");
    device_test->AddDevice("/device/stub_cellular_device1",
                           shill::kTypeCellular, "stub_cellular_device1");

    const bool add_to_watchlist = true;
    const bool add_to_visible = true;
    service_test->AddService("stub_ethernet", "eth0",
                             shill::kTypeEthernet, shill::kStateOnline,
                             add_to_visible, add_to_watchlist);
    service_test->SetServiceProperty(
        "stub_ethernet",
        shill::kProfileProperty,
        base::StringValue(ShillProfileClient::GetSharedProfilePath()));
    profile_test->AddService(ShillProfileClient::GetSharedProfilePath(),
                             "stub_ethernet");

    service_test->AddService("stub_wifi1", "wifi1",
                             shill::kTypeWifi, shill::kStateOnline,
                             add_to_visible, add_to_watchlist);
    service_test->SetServiceProperty("stub_wifi1",
                                     shill::kSecurityProperty,
                                     base::StringValue(shill::kSecurityWep));
    service_test->SetServiceProperty("stub_wifi1",
                                     shill::kProfileProperty,
                                     base::StringValue(kUser1ProfilePath));
    profile_test->AddService(kUser1ProfilePath, "stub_wifi1");
    base::ListValue frequencies1;
    frequencies1.AppendInteger(2400);
    service_test->SetServiceProperty("stub_wifi1",
                                     shill::kWifiFrequencyListProperty,
                                     frequencies1);
    service_test->SetServiceProperty("stub_wifi1",
                                     shill::kWifiFrequency,
                                     base::FundamentalValue(2400));

    service_test->AddService("stub_wifi2", "wifi2_PSK",
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible, add_to_watchlist);
    service_test->SetServiceProperty("stub_wifi2",
                                     shill::kGuidProperty,
                                     base::StringValue("stub_wifi2"));
    service_test->SetServiceProperty("stub_wifi2",
                                     shill::kSecurityProperty,
                                     base::StringValue(shill::kSecurityPsk));
    service_test->SetServiceProperty("stub_wifi2",
                                     shill::kSignalStrengthProperty,
                                     base::FundamentalValue(80));
    service_test->SetServiceProperty("stub_wifi2",
                                     shill::kConnectableProperty,
                                     base::FundamentalValue(true));

    base::ListValue frequencies2;
    frequencies2.AppendInteger(2400);
    frequencies2.AppendInteger(5000);
    service_test->SetServiceProperty("stub_wifi2",
                                     shill::kWifiFrequencyListProperty,
                                     frequencies2);
    service_test->SetServiceProperty("stub_wifi2",
                                     shill::kWifiFrequency,
                                     base::FundamentalValue(5000));
    service_test->SetServiceProperty("stub_wifi2",
                                     shill::kProfileProperty,
                                     base::StringValue(kUser1ProfilePath));
    profile_test->AddService(kUser1ProfilePath, "stub_wifi2");

    service_test->AddService("stub_cellular1", "cellular1",
                             shill::kTypeCellular, shill::kStateIdle,
                             add_to_visible, add_to_watchlist);
    service_test->SetServiceProperty(
        "stub_cellular1",
        shill::kNetworkTechnologyProperty,
        base::StringValue(shill::kNetworkTechnologyGsm));
    service_test->SetServiceProperty(
        "stub_cellular1",
        shill::kActivationStateProperty,
        base::StringValue(shill::kActivationStateNotActivated));
    service_test->SetServiceProperty(
        "stub_cellular1",
        shill::kRoamingStateProperty,
        base::StringValue(shill::kRoamingStateHome));

    service_test->AddService("stub_vpn1", "vpn1",
                             shill::kTypeVPN,
                             shill::kStateOnline,
                             add_to_visible, add_to_watchlist);

    manager_test->SortManagerServices();

    content::RunAllPendingInMessageLoop();
  }
#else  // !defined(OS_CHROMEOS)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Whitelist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");
  }

  static KeyedService* CreateNetworkingPrivateServiceClient(
      content::BrowserContext* profile) {
    return new extensions::NetworkingPrivateServiceClient(
        wifi::WiFiService::CreateForTest(),
        new CryptoVerifyStub());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();
    NetworkingPrivateServiceClientFactory::GetInstance()->SetTestingFactory(
        profile(),
        &CreateNetworkingPrivateServiceClient);
  }

#endif  // OS_CHROMEOS

 protected:
#if defined(OS_CHROMEOS)
  policy::MockConfigurationPolicyProvider provider_;
  std::string userhash_;
#endif
};

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other.

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest, StartConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest, StartDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       StartConnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       StartDisconnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       StartGetPropertiesNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startGetPropertiesNonexistent"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest, GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       GetVisibleNetworksWifi) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworksWifi")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest, RequestNetworkScan) {
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScan")) << message_;
}

// Properties are filtered and translated through
// ShillToONCTranslator::TranslateWiFiWithState
IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest, GetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest, GetState) {
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest, GetStateNonExistent) {
  EXPECT_TRUE(RunNetworkingSubtest("getStateNonExistent")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest, SetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setProperties")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest, CreateNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("createNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       GetManagedProperties) {
#if defined(OS_CHROMEOS)
  // TODO(mef): Move this to ChromeOS-specific helper or SetUpOnMainThread.
  ShillServiceClient::TestInterface* service_test =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
  const std::string uidata_blob =
      "{ \"user_settings\": {"
      "      \"WiFi\": {"
      "        \"Passphrase\": \"FAKE_CREDENTIAL_VPaJDV9x\" }"
      "    }"
      "}";
  service_test->SetServiceProperty("stub_wifi2",
                                   shill::kUIDataProperty,
                                   base::StringValue(uidata_blob));
  service_test->SetServiceProperty("stub_wifi2",
                                   shill::kAutoConnectProperty,
                                   base::FundamentalValue(false));

  ShillProfileClient::TestInterface* profile_test =
      DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
  // Update the profile entry.
  profile_test->AddService(kUser1ProfilePath, "stub_wifi2");

  content::RunAllPendingInMessageLoop();

  const std::string user_policy_blob =
      "{ \"NetworkConfigurations\": ["
      "    { \"GUID\": \"stub_wifi2\","
      "      \"Type\": \"WiFi\","
      "      \"Name\": \"My WiFi Network\","
      "      \"WiFi\": {"
      "        \"Passphrase\": \"passphrase\","
      "        \"Recommended\": [ \"AutoConnect\", \"Passphrase\" ],"
      "        \"SSID\": \"wifi2_PSK\","
      "        \"Security\": \"WPA-PSK\" }"
      "    }"
      "  ],"
      "  \"Certificates\": [],"
      "  \"Type\": \"UnencryptedConfiguration\""
      "}";

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             new base::StringValue(user_policy_blob),
             NULL);
  provider_.UpdateChromePolicy(policy);

  content::RunAllPendingInMessageLoop();
#endif  // OS_CHROMEOS

  EXPECT_TRUE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       OnNetworksChangedEventConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventConnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       OnNetworksChangedEventDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventDisconnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       OnNetworkListChangedEvent) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworkListChangedEvent")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       VerifyDestination) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyDestination")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       VerifyAndEncryptCredentials) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       VerifyAndEncryptData) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptData")) << message_;
}

#if defined(OS_CHROMEOS)
// Currently TDLS support is only enabled for Chrome OS.
IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       SetWifiTDLSEnabledState) {
  EXPECT_TRUE(RunNetworkingSubtest("setWifiTDLSEnabledState")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       GetWifiTDLSStatus) {
  EXPECT_TRUE(RunNetworkingSubtest("getWifiTDLSStatus")) << message_;
}
#endif

// NetworkPortalDetector is only enabled for Chrome OS.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ExtensionNetworkingPrivateApiTest,
                       GetCaptivePortalStatus) {
  NetworkPortalDetectorTestImpl* detector = new NetworkPortalDetectorTestImpl();
  NetworkPortalDetector::InitializeForTesting(detector);
  NetworkPortalDetector::CaptivePortalState state;
  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  detector->SetDetectionResultsForTesting("stub_ethernet", state);

  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE;
  detector->SetDetectionResultsForTesting("stub_wifi1", state);

  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  detector->SetDetectionResultsForTesting("stub_wifi2", state);

  state.status =
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
  detector->SetDetectionResultsForTesting("stub_cellular1", state);

  EXPECT_TRUE(RunNetworkingSubtest("getCaptivePortalStatus")) << message_;
}
#endif  // defined(OS_CHROMEOS)

INSTANTIATE_TEST_CASE_P(ExtensionNetworkingPrivateApiTestInstantiation,
                        ExtensionNetworkingPrivateApiTest,
                        testing::Bool());

}  // namespace
