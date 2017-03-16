// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/networking_cast_private/chrome_networking_cast_private_delegate.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"
#include "extensions/common/api/networking_private.h"
#include "extensions/common/switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif

namespace extensions {

namespace {

class TestVerifyDelegate : public NetworkingPrivateDelegate::VerifyDelegate {
 public:
  TestVerifyDelegate() = default;
  ~TestVerifyDelegate() override = default;

  void VerifyDestination(const VerificationProperties& properties,
                         const BoolCallback& success_callback,
                         const FailureCallback& failure_callback) override {
    success_callback.Run(true);
  }

  void VerifyAndEncryptCredentials(
      const std::string& guid,
      const VerificationProperties& properties,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override {
    success_callback.Run("encrypted_credentials");
  }

  void VerifyAndEncryptData(const VerificationProperties& properties,
                            const std::string& data,
                            const StringCallback& success_callback,
                            const FailureCallback& failure_callback) override {
    success_callback.Run("encrypted_data");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestVerifyDelegate);
};

}  // namespace

class NetworkingCastPrivateApiTest : public ExtensionApiTest {
 public:
  NetworkingCastPrivateApiTest() = default;
  ~NetworkingCastPrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Whitelist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");
  }

  void SetUp() override {
    networking_cast_private_delegate_factory_ = base::Bind(
        &NetworkingCastPrivateApiTest::CreateNetworkingCastPrivateDelegate,
        base::Unretained(this));
    ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(
        &networking_cast_private_delegate_factory_);

    ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager* dbus_manager =
        chromeos::DBusThreadManager::Get();
    chromeos::ShillDeviceClient::TestInterface* device_test =
        dbus_manager->GetShillDeviceClient()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice("/device/stub_wifi_device1", shill::kTypeWifi,
                           "stub_wifi_device");
    device_test->SetTDLSState(shill::kTDLSConnectedState);

    chromeos::ShillServiceClient::TestInterface* service_test =
        dbus_manager->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService("stub_wifi", "stub_wifi_guid", "wifi",
                             shill::kTypeWifi, shill::kStateOnline,
                             true /* add_to_visible */);
#endif  // defined(OS_CHROMEOS)
  }

  void TearDown() override {
    ExtensionApiTest::TearDown();
    ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(nullptr);
  }

  bool TdlsSupported() {
#if defined(OS_CHROMEOS)
    return true;
#else
    return false;
#endif
  }

 private:
  std::unique_ptr<ChromeNetworkingCastPrivateDelegate>
  CreateNetworkingCastPrivateDelegate() {
    return std::unique_ptr<ChromeNetworkingCastPrivateDelegate>(
        new ChromeNetworkingCastPrivateDelegate(
            base::MakeUnique<TestVerifyDelegate>()));
  }

  ChromeNetworkingCastPrivateDelegate::FactoryCallback
      networking_cast_private_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingCastPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(NetworkingCastPrivateApiTest, Basic) {
  const std::string arg =
      base::StringPrintf("{\"tdlsSupported\": %d}", TdlsSupported());
  EXPECT_TRUE(RunPlatformAppTestWithArg("networking_cast_private", arg.c_str()))
      << message_;
}

}  // namespace extensions
