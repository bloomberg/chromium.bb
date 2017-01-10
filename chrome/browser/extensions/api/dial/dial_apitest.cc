// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/dial/dial_api.h"
#include "chrome/browser/extensions/api/dial/dial_api_factory.h"
#include "chrome/browser/extensions/api/dial/dial_registry.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using extensions::Extension;
using extensions::ResultCatcher;
using extensions::api::dial::DialDeviceData;
using extensions::api::dial::DialDeviceDescriptionData;
using extensions::api::dial::DialRegistry;

namespace {

class DialAPITest : public ExtensionApiTest {
 public:
  DialAPITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }
};

}  // namespace

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_DeviceListEvents DISABLED_DeviceListEvents
#else
#define MAYBE_DeviceListEvents DeviceListEvents
#endif
// Test receiving DIAL API events.
IN_PROC_BROWSER_TEST_F(DialAPITest, MAYBE_DeviceListEvents) {
  // Setup the test.
  ASSERT_TRUE(RunExtensionSubtest("dial/experimental", "device_list.html"));

  // Send three device list updates.
  scoped_refptr<extensions::DialAPI> api =
      extensions::DialAPIFactory::GetInstance()->GetForBrowserContext(
          profile());
  ASSERT_TRUE(api.get());
  DialRegistry::DeviceList devices;

  ResultCatcher catcher;

  DialDeviceData device1;
  device1.set_device_id("1");
  device1.set_label("1");
  device1.set_device_description_url(GURL("http://127.0.0.1/dd.xml"));

  devices.push_back(device1);
  api->SendEventOnUIThread(devices);

  DialDeviceData device2;
  device2.set_device_id("2");
  device2.set_label("2");
  device2.set_device_description_url(GURL("http://127.0.0.2/dd.xml"));

  devices.push_back(device2);
  api->SendEventOnUIThread(devices);

  DialDeviceData device3;
  device3.set_device_id("3");
  device3.set_label("3");
  device3.set_device_description_url(GURL("http://127.0.0.3/dd.xml"));

  devices.push_back(device3);
  api->SendEventOnUIThread(devices);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(DialAPITest, Discovery) {
  ASSERT_TRUE(RunExtensionSubtest("dial/experimental", "discovery.html"));
}

// discoverNow does not do discovery when there are no listeners; in that case
// the DIAL service will not be active.
IN_PROC_BROWSER_TEST_F(DialAPITest, DiscoveryNoListeners) {
  ASSERT_TRUE(RunExtensionSubtest("dial/experimental",
                                  "discovery_no_listeners.html"));
}

// Make sure this API is only accessible to whitelisted extensions.
IN_PROC_BROWSER_TEST_F(DialAPITest, NonWhitelistedExtension) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ExtensionTestMessageListener listener("ready", true);
  const Extension* extension =
      LoadExtensionWithFlags(test_data_dir_.AppendASCII("dial/whitelist"),
                             ExtensionBrowserTest::kFlagIgnoreManifestWarnings);
  // We should have a DIAL API not available warning.
  ASSERT_FALSE(extension->install_warnings().empty());

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(DialAPITest, OnError) {
  ASSERT_TRUE(RunExtensionSubtest("dial/experimental", "on_error.html"));
}

IN_PROC_BROWSER_TEST_F(DialAPITest, FetchDeviceDescription) {
  scoped_refptr<extensions::DialAPI> api =
      extensions::DialAPIFactory::GetInstance()->GetForBrowserContext(
          profile());
  ASSERT_TRUE(api);

  DialDeviceData test_device("testDeviceId",
                             GURL("http://127.0.0.1/description.xml"),
                             base::Time::Now());
  test_device.set_label("testDevice");

  DialDeviceDescriptionData test_description("<xml>testDescription</xml>",
                                             GURL("http://127.0.0.1/apps"));
  api->SetDeviceForTest(test_device, test_description);

  ASSERT_TRUE(RunExtensionSubtest("dial/experimental",
                                  "fetch_device_description.html"));
}
