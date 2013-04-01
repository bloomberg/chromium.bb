// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class ExtensionNetworkingPrivateApiTest : public ExtensionApiTest {
 public:
  // Whitelist the extension ID of the test extension.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, "epcifkihnkjgphfkloaaleeakhpmgdmn");
 }

  bool RunNetworkingSubtest(const std::string& subtest) {
    return RunExtensionSubtest(
        "networking", "main.html?" + subtest,
        kFlagEnableFileAccess | kFlagLoadAsComponent);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    ShillDeviceClient::TestInterface* device_test =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice("/device/stub_wifi_device1",
                           flimflam::kTypeWifi, "stub_wifi_device1");
    device_test->AddDevice("/device/stub_cellular_device1",
                           flimflam::kTypeCellular, "stub_cellular_device1");

    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    const bool add_to_watchlist = true;
    service_test->AddService("stub_ethernet", "eth0",
                             flimflam::kTypeEthernet, flimflam::kStateOnline,
                             add_to_watchlist);

    service_test->AddService("stub_wifi1", "wifi1",
                             flimflam::kTypeWifi, flimflam::kStateOnline,
                             add_to_watchlist);
    service_test->SetServiceProperty("stub_wifi1",
                                     flimflam::kSecurityProperty,
                                     base::StringValue(flimflam::kSecurityWep));

    service_test->AddService("stub_wifi2", "wifi2_PSK",
                             flimflam::kTypeWifi, flimflam::kStateIdle,
                             add_to_watchlist);
    service_test->SetServiceProperty("stub_wifi2",
                                     flimflam::kSecurityProperty,
                                     base::StringValue(flimflam::kSecurityPsk));
    service_test->SetServiceProperty("stub_wifi2",
                                     flimflam::kSignalStrengthProperty,
                                     base::FundamentalValue(80));

    service_test->AddService("stub_cellular1", "cellular1",
                             flimflam::kTypeCellular, flimflam::kStateIdle,
                             add_to_watchlist);
    service_test->SetServiceProperty(
        "stub_cellular1",
        flimflam::kNetworkTechnologyProperty,
        base::StringValue(flimflam::kNetworkTechnologyGsm));
    service_test->SetServiceProperty(
        "stub_cellular1",
        flimflam::kActivationStateProperty,
        base::StringValue(flimflam::kActivationStateNotActivated));
    service_test->SetServiceProperty(
        "stub_cellular1",
        flimflam::kRoamingStateProperty,
        base::StringValue(flimflam::kRoamingStateHome));

    service_test->AddService("stub_vpn1", "vpn1",
                             flimflam::kTypeVPN,
                             flimflam::kStateOnline,
                             add_to_watchlist);
  }
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

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       GetVisibleNetworksWifi) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworksWifi")) << message_;
}

// Properties are filtered and translated through
// ShillToONCTranslator::TranslateWiFiWithState
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetState) {
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, SetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setProperties")) << message_;
}

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

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       OnNetworkListChangedEvent) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworkListChangedEvent")) << message_;
}

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


}  // namespace chromeos
