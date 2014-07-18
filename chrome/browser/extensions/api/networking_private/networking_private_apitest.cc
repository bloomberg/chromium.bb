// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/login/user_names.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "policy/policy_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#else  // !defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client_factory.h"
#include "components/wifi/fake_wifi_service.h"
#endif  // defined(OS_CHROMEOS)

// TODO(stevenjb/mef): Clean these tests up. crbug.com/371442

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
using chromeos::ShillIPConfigClient;
using chromeos::ShillManagerClient;
using chromeos::ShillProfileClient;
using chromeos::ShillServiceClient;
#else  // !defined(OS_CHROMEOS)
using extensions::NetworkingPrivateServiceClientFactory;
#endif  // defined(OS_CHROMEOS)

namespace {

#if defined(OS_CHROMEOS)
const char kUser1ProfilePath[] = "/profile/user1/shill";
const char kWifiDevicePath[] = "/device/stub_wifi_device1";
const char kCellularDevicePath[] = "/device/stub_cellular_device1";
const char kIPConfigPath[] = "/ipconfig/ipconfig1";

class TestListener : public content::NotificationObserver {
 public:
  TestListener(const std::string& message, const base::Closure& callback)
      : message_(message), callback_(callback) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  virtual void Observe(int type,
                       const content::NotificationSource& /* source */,
                       const content::NotificationDetails& details) OVERRIDE {
    const std::string& message = *content::Details<std::string>(details).ptr();
    if (message == message_)
      callback_.Run();
  }

 private:
  std::string message_;
  base::Closure callback_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TestListener);
};
#else  // !defined(OS_CHROMEOS)

// Stub Verify* methods implementation to satisfy expectations of
// networking_private_apitest.
// TODO(mef): Fix ChromeOS implementation to use NetworkingPrivateCrypto,
// and update networking_private_apitest to use and expect valid data.
// That will eliminate the need for mock implementation.
class CryptoVerifyStub
    : public extensions::NetworkingPrivateServiceClient::CryptoVerify {
  virtual void VerifyDestination(const Credentials& verification_properties,
                                 bool* verified,
                                 std::string* error) OVERRIDE {
    *verified = true;
  }

  virtual void VerifyAndEncryptCredentials(
      const std::string& network_guid,
      const Credentials& credentials,
      const VerifyAndEncryptCredentialsCallback& callback) OVERRIDE {
    callback.Run("encrypted_credentials", "");
  }

  virtual void VerifyAndEncryptData(const Credentials& verification_properties,
                                    const std::string& data,
                                    std::string* base64_encoded_ciphertext,
                                    std::string* error) OVERRIDE {
    *base64_encoded_ciphertext = "encrypted_data";
  }
};
#endif  // defined(OS_CHROMEOS)

class ExtensionNetworkingPrivateApiTest
    : public ExtensionApiTest {
 public:
  ExtensionNetworkingPrivateApiTest()
#if defined(OS_CHROMEOS)
      : detector_(NULL),
        service_test_(NULL),
        manager_test_(NULL),
        device_test_(NULL)
#endif
  {
  }

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
    const std::string login_user = chromeos::login::CanonicalizeUserID(
        command_line->GetSwitchValueNative(chromeos::switches::kLoginUser));
    const std::string sanitized_user =
        CryptohomeClient::GetStubSanitizedUsername(login_user);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    sanitized_user);
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

  void SetupCellular() {
    // Add a Cellular Device and set a couple of properties.
    device_test_->AddDevice(
        kCellularDevicePath, shill::kTypeCellular, "stub_cellular_device1");
    device_test_->SetDeviceProperty(kCellularDevicePath,
                                    shill::kCarrierProperty,
                                    base::StringValue("Cellular1_Carrier"));
    base::DictionaryValue home_provider;
    home_provider.SetString("name", "Cellular1_Provider");
    home_provider.SetString("country", "us");
    device_test_->SetDeviceProperty(kCellularDevicePath,
                                    shill::kHomeProviderProperty,
                                    home_provider);
    AddService("stub_cellular1", "cellular1",
               shill::kTypeCellular, shill::kStateIdle);
    // Note: These properties will show up in a "Cellular" object in ONC.
    service_test_->SetServiceProperty(
        "stub_cellular1",
        shill::kNetworkTechnologyProperty,
        base::StringValue(shill::kNetworkTechnologyGsm));
    service_test_->SetServiceProperty(
        "stub_cellular1",
        shill::kActivationStateProperty,
        base::StringValue(shill::kActivationStateNotActivated));
    service_test_->SetServiceProperty(
        "stub_cellular1",
        shill::kRoamingStateProperty,
        base::StringValue(shill::kRoamingStateHome));
    content::RunAllPendingInMessageLoop();
  }

  void AddService(const std::string& service_path,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state) {
    service_test_->AddService(
        service_path, service_path + "_guid", name,
        type, state, true /* add_to_visible */);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    detector_ = new NetworkPortalDetectorTestImpl();
    NetworkPortalDetector::InitializeForTesting(detector_);

    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    InitializeSanitizedUsername();

    DBusThreadManager* dbus_manager = DBusThreadManager::Get();
    manager_test_ = dbus_manager->GetShillManagerClient()->GetTestInterface();
    service_test_ = dbus_manager->GetShillServiceClient()->GetTestInterface();
    device_test_ = dbus_manager->GetShillDeviceClient()->GetTestInterface();

    ShillIPConfigClient::TestInterface* ip_config_test =
        dbus_manager->GetShillIPConfigClient()->GetTestInterface();
    ShillProfileClient::TestInterface* profile_test =
        dbus_manager->GetShillProfileClient()->GetTestInterface();

    device_test_->ClearDevices();
    service_test_->ClearServices();

    // Sends a notification about the added profile.
    profile_test->AddProfile(kUser1ProfilePath, userhash_);

    // Add IPConfigs
    base::DictionaryValue ipconfig;
    ipconfig.SetStringWithoutPathExpansion(shill::kAddressProperty, "0.0.0.0");
    ipconfig.SetStringWithoutPathExpansion(shill::kGatewayProperty, "0.0.0.1");
    ipconfig.SetIntegerWithoutPathExpansion(shill::kPrefixlenProperty, 0);
    ipconfig.SetStringWithoutPathExpansion(shill::kMethodProperty,
                                           shill::kTypeIPv4);
    ip_config_test->AddIPConfig(kIPConfigPath, ipconfig);

    // Add Devices
    device_test_->AddDevice(
        kWifiDevicePath, shill::kTypeWifi, "stub_wifi_device1");
    base::ListValue wifi_ip_configs;
    wifi_ip_configs.AppendString(kIPConfigPath);
    device_test_->SetDeviceProperty(
        kWifiDevicePath, shill::kIPConfigsProperty, wifi_ip_configs);
    device_test_->SetDeviceProperty(kWifiDevicePath,
                                    shill::kAddressProperty,
                                    base::StringValue("001122aabbcc"));

    // Add Services
    AddService("stub_ethernet", "eth0",
               shill::kTypeEthernet, shill::kStateOnline);
    service_test_->SetServiceProperty(
        "stub_ethernet",
        shill::kProfileProperty,
        base::StringValue(ShillProfileClient::GetSharedProfilePath()));
    profile_test->AddService(ShillProfileClient::GetSharedProfilePath(),
                             "stub_ethernet");

    AddService("stub_wifi1", "wifi1", shill::kTypeWifi, shill::kStateOnline);
    service_test_->SetServiceProperty("stub_wifi1",
                                      shill::kSecurityProperty,
                                      base::StringValue(shill::kSecurityWep));
    service_test_->SetServiceProperty("stub_wifi1",
                                      shill::kSignalStrengthProperty,
                                      base::FundamentalValue(40));
    service_test_->SetServiceProperty("stub_wifi1",
                                      shill::kProfileProperty,
                                      base::StringValue(kUser1ProfilePath));
    service_test_->SetServiceProperty("stub_wifi1",
                                      shill::kConnectableProperty,
                                      base::FundamentalValue(true));
    service_test_->SetServiceProperty("stub_wifi1",
                                      shill::kDeviceProperty,
                                      base::StringValue(kWifiDevicePath));
    profile_test->AddService(kUser1ProfilePath, "stub_wifi1");
    base::ListValue frequencies1;
    frequencies1.AppendInteger(2400);
    service_test_->SetServiceProperty("stub_wifi1",
                                      shill::kWifiFrequencyListProperty,
                                      frequencies1);
    service_test_->SetServiceProperty("stub_wifi1",
                                      shill::kWifiFrequency,
                                      base::FundamentalValue(2400));

    AddService("stub_wifi2", "wifi2_PSK", shill::kTypeWifi, shill::kStateIdle);
    service_test_->SetServiceProperty("stub_wifi2",
                                      shill::kSecurityProperty,
                                      base::StringValue(shill::kSecurityPsk));
    service_test_->SetServiceProperty("stub_wifi2",
                                      shill::kSignalStrengthProperty,
                                      base::FundamentalValue(80));
    service_test_->SetServiceProperty("stub_wifi2",
                                      shill::kConnectableProperty,
                                      base::FundamentalValue(true));

    base::ListValue frequencies2;
    frequencies2.AppendInteger(2400);
    frequencies2.AppendInteger(5000);
    service_test_->SetServiceProperty("stub_wifi2",
                                      shill::kWifiFrequencyListProperty,
                                      frequencies2);
    service_test_->SetServiceProperty("stub_wifi2",
                                      shill::kWifiFrequency,
                                      base::FundamentalValue(5000));
    service_test_->SetServiceProperty("stub_wifi2",
                                      shill::kProfileProperty,
                                      base::StringValue(kUser1ProfilePath));
    profile_test->AddService(kUser1ProfilePath, "stub_wifi2");

    AddService("stub_vpn1", "vpn1", shill::kTypeVPN, shill::kStateOnline);

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
        new wifi::FakeWiFiService(), new CryptoVerifyStub());
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
  NetworkPortalDetectorTestImpl* detector() { return detector_; }

  NetworkPortalDetectorTestImpl* detector_;
  ShillServiceClient::TestInterface* service_test_;
  ShillManagerClient::TestInterface* manager_test_;
  ShillDeviceClient::TestInterface* device_test_;
  policy::MockConfigurationPolicyProvider provider_;
  std::string userhash_;
#endif
};

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other.

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, StartConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, StartDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       StartConnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       StartDisconnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       StartGetPropertiesNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startGetPropertiesNonexistent"))
      << message_;
}

#if defined(OS_CHROMEOS)
// TODO(stevenjb/mef): Fix these on non-Chrome OS, crbug.com/371442.
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetNetworks) {
  // Hide stub_wifi2.
  service_test_->SetServiceProperty("stub_wifi2",
                                    shill::kVisibleProperty,
                                    base::FundamentalValue(false));
  // Add a couple of additional networks that are not configured (saved).
  AddService("stub_wifi3", "wifi3", shill::kTypeWifi, shill::kStateIdle);
  AddService("stub_wifi4", "wifi4", shill::kTypeWifi, shill::kStateIdle);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(RunNetworkingSubtest("getNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       GetVisibleNetworksWifi) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworksWifi")) << message_;
}

#if defined(OS_CHROMEOS)
// TODO(stevenjb/mef): Fix this on non-Chrome OS, crbug.com/371442.
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, RequestNetworkScan) {
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScan")) << message_;
}
#endif

// Properties are filtered and translated through
// ShillToONCTranslator::TranslateWiFiWithState
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       GetCellularProperties) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("getPropertiesCellular")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetState) {
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetStateNonExistent) {
  EXPECT_TRUE(RunNetworkingSubtest("getStateNonExistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, SetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, CreateNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("createNetwork")) << message_;
}

#if defined(OS_CHROMEOS)
// TODO(stevenjb/mef): Find a maintainable way to support this on win/mac and
// a better way to set this up on Chrome OS.
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       GetManagedProperties) {
  const std::string uidata_blob =
      "{ \"user_settings\": {"
      "      \"WiFi\": {"
      "        \"Passphrase\": \"FAKE_CREDENTIAL_VPaJDV9x\" }"
      "    }"
      "}";
  service_test_->SetServiceProperty("stub_wifi2",
                                    shill::kUIDataProperty,
                                    base::StringValue(uidata_blob));
  service_test_->SetServiceProperty("stub_wifi2",
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

  EXPECT_TRUE(RunNetworkingSubtest("getManagedProperties")) << message_;
}
#endif  // OS_CHROMEOS

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       OnNetworksChangedEventConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventConnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       OnNetworksChangedEventDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventDisconnect"))
      << message_;
}

#if defined(OS_CHROMEOS)
// TODO(stevenjb/mef): Fix this on non-Chrome OS, crbug.com/371442.
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       OnNetworkListChangedEvent) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworkListChangedEvent")) << message_;
}
#endif  // OS_CHROMEOS

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       VerifyDestination) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyDestination")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       VerifyAndEncryptCredentials) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       VerifyAndEncryptData) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptData")) << message_;
}

#if defined(OS_CHROMEOS)
// Currently TDLS support is only enabled for Chrome OS.
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       SetWifiTDLSEnabledState) {
  EXPECT_TRUE(RunNetworkingSubtest("setWifiTDLSEnabledState")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       GetWifiTDLSStatus) {
  EXPECT_TRUE(RunNetworkingSubtest("getWifiTDLSStatus")) << message_;
}
#endif

// NetworkPortalDetector is only enabled for Chrome OS.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       GetCaptivePortalStatus) {
  SetupCellular();

  NetworkPortalDetector::CaptivePortalState state;
  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  detector()->SetDetectionResultsForTesting("stub_ethernet_guid", state);

  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE;
  detector()->SetDetectionResultsForTesting("stub_wifi1_guid", state);

  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  detector()->SetDetectionResultsForTesting("stub_wifi2_guid", state);

  state.status =
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
  detector()->SetDetectionResultsForTesting("stub_cellular1_guid", state);

  EXPECT_TRUE(RunNetworkingSubtest("getCaptivePortalStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       CaptivePortalNotification) {
  detector()->SetDefaultNetworkForTesting("wifi_guid");
  NetworkPortalDetector::CaptivePortalState state;
  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  detector()->SetDetectionResultsForTesting("wifi_guid", state);

  TestListener listener(
      "notifyPortalDetectorObservers",
      base::Bind(&NetworkPortalDetectorTestImpl::NotifyObserversForTesting,
                 base::Unretained(detector())));
  EXPECT_TRUE(RunNetworkingSubtest("captivePortalNotification")) << message_;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace
