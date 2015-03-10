// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"
#include "chrome/browser/extensions/extension_apitest.h"
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
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/networking_private/networking_private_chromeos.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/switches.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// This tests the Chrome OS implementation of the networkingPrivate API
// (NetworkingPrivateChromeOS). Note: The test expectations for chromeos, and
// win/mac (NetworkingPrivateServiceClient) are different to reflect the
// different implementations, but should be kept similar where possible.

using testing::Return;
using testing::_;

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

using extensions::NetworkingPrivateDelegate;
using extensions::NetworkingPrivateDelegateFactory;
using extensions::NetworkingPrivateChromeOS;

namespace {

const char kUser1ProfilePath[] = "/profile/user1/shill";
const char kWifiDevicePath[] = "/device/stub_wifi_device1";
const char kCellularDevicePath[] = "/device/stub_cellular_device1";
const char kIPConfigPath[] = "/ipconfig/ipconfig1";

const char kWifi1ServicePath[] = "stub_wifi1";
const char kWifi2ServicePath[] = "stub_wifi2";
const char kCellular1ServicePath[] = "stub_cellular1";

// Stub Verify* methods implementation to satisfy expectations of
// networking_private_apitest.
class CryptoVerifyStub : public NetworkingPrivateDelegate::VerifyDelegate {
  void VerifyDestination(const VerificationProperties& verification_properties,
                         const BoolCallback& success_callback,
                         const FailureCallback& failure_callback) override {
    success_callback.Run(true);
  }

  void VerifyAndEncryptCredentials(
      const std::string& guid,
      const VerificationProperties& verification_properties,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override {
    success_callback.Run("encrypted_credentials");
  }

  void VerifyAndEncryptData(
      const VerificationProperties& verification_properties,
      const std::string& data,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override {
    success_callback.Run("encrypted_data");
  }
};

class TestListener : public content::NotificationObserver {
 public:
  TestListener(const std::string& message, const base::Closure& callback)
      : message_(message), callback_(callback) {
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& /* source */,
               const content::NotificationDetails& details) override {
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

class NetworkingPrivateChromeOSApiTest : public ExtensionApiTest {
 public:
  NetworkingPrivateChromeOSApiTest()
      : detector_(nullptr),
        service_test_(nullptr),
        manager_test_(nullptr),
        device_test_(nullptr) {}

  bool RunNetworkingSubtest(const std::string& subtest) {
    return RunExtensionSubtest("networking_private/chromeos",
                               "main.html?" + subtest,
                               kFlagEnableFileAccess | kFlagLoadAsComponent);
  }

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  static void AssignString(std::string* out,
                           DBusMethodCallStatus call_status,
                           const std::string& result) {
    CHECK_EQ(call_status, DBUS_METHOD_CALL_SUCCESS);
    *out = result;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
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
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    user_manager::User* user = user_manager->GetActiveUser();
    CHECK(user);
    std::string userhash;
    DBusThreadManager::Get()->GetCryptohomeClient()->GetSanitizedUsername(
        user->email(), base::Bind(&AssignString, &userhash_));
    content::RunAllPendingInMessageLoop();
    CHECK(!userhash_.empty());
  }

  void SetupCellular() {
    // Add a Cellular Device and set a couple of properties.
    device_test_->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                            "stub_cellular_device1");
    device_test_->SetDeviceProperty(kCellularDevicePath,
                                    shill::kCarrierProperty,
                                    base::StringValue("Cellular1_Carrier"));
    base::DictionaryValue home_provider;
    home_provider.SetString("name", "Cellular1_Provider");
    home_provider.SetString("country", "us");
    device_test_->SetDeviceProperty(
        kCellularDevicePath, shill::kHomeProviderProperty, home_provider);
    AddService(kCellular1ServicePath, "cellular1", shill::kTypeCellular,
               shill::kStateIdle);
    // Note: These properties will show up in a "Cellular" object in ONC.
    service_test_->SetServiceProperty(kCellular1ServicePath,
                                      shill::kAutoConnectProperty,
                                      base::FundamentalValue(true));
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kNetworkTechnologyProperty,
        base::StringValue(shill::kNetworkTechnologyGsm));
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kActivationStateProperty,
        base::StringValue(shill::kActivationStateNotActivated));
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kRoamingStateProperty,
        base::StringValue(shill::kRoamingStateHome));
    content::RunAllPendingInMessageLoop();
  }

  void AddService(const std::string& service_path,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state) {
    service_test_->AddService(service_path, service_path + "_guid", name, type,
                              state, true /* add_to_visible */);
  }

  static KeyedService* CreateNetworkingPrivateServiceClient(
      content::BrowserContext* profile) {
    scoped_ptr<CryptoVerifyStub> crypto_verify(new CryptoVerifyStub);
    return new NetworkingPrivateChromeOS(profile, crypto_verify.Pass());
  }

  void SetUpOnMainThread() override {
    detector_ = new NetworkPortalDetectorTestImpl();
    NetworkPortalDetector::InitializeForTesting(detector_);

    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    NetworkingPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), &CreateNetworkingPrivateServiceClient);

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

    // Enable technologies.
    manager_test_->AddTechnology("wimax", true);

    // Add IPConfigs
    base::DictionaryValue ipconfig;
    ipconfig.SetStringWithoutPathExpansion(shill::kAddressProperty, "0.0.0.0");
    ipconfig.SetStringWithoutPathExpansion(shill::kGatewayProperty, "0.0.0.1");
    ipconfig.SetIntegerWithoutPathExpansion(shill::kPrefixlenProperty, 0);
    ipconfig.SetStringWithoutPathExpansion(shill::kMethodProperty,
                                           shill::kTypeIPv4);
    ip_config_test->AddIPConfig(kIPConfigPath, ipconfig);

    // Add Devices
    device_test_->AddDevice(kWifiDevicePath, shill::kTypeWifi,
                            "stub_wifi_device1");
    base::ListValue wifi_ip_configs;
    wifi_ip_configs.AppendString(kIPConfigPath);
    device_test_->SetDeviceProperty(kWifiDevicePath, shill::kIPConfigsProperty,
                                    wifi_ip_configs);
    device_test_->SetDeviceProperty(kWifiDevicePath, shill::kAddressProperty,
                                    base::StringValue("001122aabbcc"));

    // Add Services
    AddService("stub_ethernet", "eth0", shill::kTypeEthernet,
               shill::kStateOnline);
    service_test_->SetServiceProperty(
        "stub_ethernet", shill::kProfileProperty,
        base::StringValue(ShillProfileClient::GetSharedProfilePath()));
    profile_test->AddService(ShillProfileClient::GetSharedProfilePath(),
                             "stub_ethernet");

    AddService(kWifi1ServicePath, "wifi1", shill::kTypeWifi,
               shill::kStateOnline);
    service_test_->SetServiceProperty(kWifi1ServicePath,
                                      shill::kSecurityClassProperty,
                                      base::StringValue(shill::kSecurityWep));
    service_test_->SetServiceProperty(kWifi1ServicePath,
                                      shill::kSignalStrengthProperty,
                                      base::FundamentalValue(40));
    service_test_->SetServiceProperty(kWifi1ServicePath,
                                      shill::kProfileProperty,
                                      base::StringValue(kUser1ProfilePath));
    service_test_->SetServiceProperty(kWifi1ServicePath,
                                      shill::kConnectableProperty,
                                      base::FundamentalValue(true));
    service_test_->SetServiceProperty(kWifi1ServicePath, shill::kDeviceProperty,
                                      base::StringValue(kWifiDevicePath));
    base::DictionaryValue static_ipconfig;
    static_ipconfig.SetStringWithoutPathExpansion(shill::kAddressProperty,
                                                  "1.2.3.4");
    service_test_->SetServiceProperty(
        kWifi1ServicePath, shill::kStaticIPConfigProperty, static_ipconfig);
    base::ListValue frequencies1;
    frequencies1.AppendInteger(2400);
    service_test_->SetServiceProperty(
        kWifi1ServicePath, shill::kWifiFrequencyListProperty, frequencies1);
    service_test_->SetServiceProperty(kWifi1ServicePath, shill::kWifiFrequency,
                                      base::FundamentalValue(2400));
    profile_test->AddService(kUser1ProfilePath, kWifi1ServicePath);

    AddService(kWifi2ServicePath, "wifi2_PSK", shill::kTypeWifi,
               shill::kStateIdle);
    service_test_->SetServiceProperty(kWifi2ServicePath,
                                      shill::kSecurityClassProperty,
                                      base::StringValue(shill::kSecurityPsk));
    service_test_->SetServiceProperty(kWifi2ServicePath,
                                      shill::kSignalStrengthProperty,
                                      base::FundamentalValue(80));
    service_test_->SetServiceProperty(kWifi2ServicePath,
                                      shill::kConnectableProperty,
                                      base::FundamentalValue(true));

    AddService("stub_wimax", "wimax", shill::kTypeWimax, shill::kStateOnline);
    service_test_->SetServiceProperty("stub_wimax",
                                      shill::kSignalStrengthProperty,
                                      base::FundamentalValue(40));
    service_test_->SetServiceProperty("stub_wimax", shill::kProfileProperty,
                                      base::StringValue(kUser1ProfilePath));
    service_test_->SetServiceProperty("stub_wimax", shill::kConnectableProperty,
                                      base::FundamentalValue(true));
    profile_test->AddService(kUser1ProfilePath, "stub_wimax");

    base::ListValue frequencies2;
    frequencies2.AppendInteger(2400);
    frequencies2.AppendInteger(5000);
    service_test_->SetServiceProperty(
        kWifi2ServicePath, shill::kWifiFrequencyListProperty, frequencies2);
    service_test_->SetServiceProperty(kWifi2ServicePath, shill::kWifiFrequency,
                                      base::FundamentalValue(5000));
    service_test_->SetServiceProperty(kWifi2ServicePath,
                                      shill::kProfileProperty,
                                      base::StringValue(kUser1ProfilePath));
    profile_test->AddService(kUser1ProfilePath, kWifi2ServicePath);

    AddService("stub_vpn1", "vpn1", shill::kTypeVPN, shill::kStateOnline);
    service_test_->SetServiceProperty(
        "stub_vpn1", shill::kProviderTypeProperty,
        base::StringValue(shill::kProviderOpenVpn));
    profile_test->AddService(kUser1ProfilePath, "stub_vpn1");

    content::RunAllPendingInMessageLoop();
  }

 protected:
  NetworkPortalDetectorTestImpl* detector() { return detector_; }

  NetworkPortalDetectorTestImpl* detector_;
  ShillServiceClient::TestInterface* service_test_;
  ShillManagerClient::TestInterface* manager_test_;
  ShillDeviceClient::TestInterface* device_test_;
  policy::MockConfigurationPolicyProvider provider_;
  std::string userhash_;
};

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other.

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartActivate) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("startActivate")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       StartConnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       StartDisconnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       StartGetPropertiesNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startGetPropertiesNonexistent"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetNetworks) {
  // Hide stub_wifi2.
  service_test_->SetServiceProperty(kWifi2ServicePath, shill::kVisibleProperty,
                                    base::FundamentalValue(false));
  // Add a couple of additional networks that are not configured (saved).
  AddService("stub_wifi3", "wifi3", shill::kTypeWifi, shill::kStateIdle);
  AddService("stub_wifi4", "wifi4", shill::kTypeWifi, shill::kStateIdle);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(RunNetworkingSubtest("getNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetVisibleNetworksWifi) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworksWifi")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, RequestNetworkScan) {
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScan")) << message_;
}

// Properties are filtered and translated through
// ShillToONCTranslator::TranslateWiFiWithState
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetCellularProperties) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("getPropertiesCellular")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetState) {
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetStateNonExistent) {
  EXPECT_TRUE(RunNetworkingSubtest("getStateNonExistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, SetWiFiProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setWiFiProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, SetVPNProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setVPNProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, CreateNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("createNetwork")) << message_;
}

// TODO(stevenjb): Find a better way to set this up on Chrome OS.
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetManagedProperties) {
  const std::string uidata_blob =
      "{ \"user_settings\": {"
      "      \"WiFi\": {"
      "        \"Passphrase\": \"FAKE_CREDENTIAL_VPaJDV9x\" }"
      "    }"
      "}";
  service_test_->SetServiceProperty(kWifi2ServicePath, shill::kUIDataProperty,
                                    base::StringValue(uidata_blob));
  service_test_->SetServiceProperty(kWifi2ServicePath,
                                    shill::kAutoConnectProperty,
                                    base::FundamentalValue(false));

  ShillProfileClient::TestInterface* profile_test =
      DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
  // Update the profile entry.
  profile_test->AddService(kUser1ProfilePath, kWifi2ServicePath);

  content::RunAllPendingInMessageLoop();

  const std::string user_policy_blob =
      "{ \"NetworkConfigurations\": ["
      "    { \"GUID\": \"stub_wifi2\","
      "      \"Type\": \"WiFi\","
      "      \"Name\": \"My WiFi Network\","
      "      \"WiFi\": {"
      "        \"HexSSID\": \"77696669325F50534B\","  // "wifi2_PSK"
      "        \"Passphrase\": \"passphrase\","
      "        \"Recommended\": [ \"AutoConnect\", \"Passphrase\" ],"
      "        \"Security\": \"WPA-PSK\" }"
      "    }"
      "  ],"
      "  \"Certificates\": [],"
      "  \"Type\": \"UnencryptedConfiguration\""
      "}";

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             new base::StringValue(user_policy_blob), nullptr);
  provider_.UpdateChromePolicy(policy);

  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnNetworksChangedEventConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventConnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnNetworksChangedEventDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventDisconnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnNetworkListChangedEvent) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworkListChangedEvent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, VerifyDestination) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyDestination")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       VerifyAndEncryptCredentials) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, VerifyAndEncryptData) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptData")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       SetWifiTDLSEnabledState) {
  device_test_->SetTDLSState(shill::kTDLSConnectedState);
  EXPECT_TRUE(RunNetworkingSubtest("setWifiTDLSEnabledState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetWifiTDLSStatus) {
  device_test_->SetTDLSState(shill::kTDLSConnectedState);
  EXPECT_TRUE(RunNetworkingSubtest("getWifiTDLSStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
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

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
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

}  // namespace
