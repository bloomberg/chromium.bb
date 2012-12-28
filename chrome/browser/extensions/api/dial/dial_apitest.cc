// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/api/dial/dial_api.h"
#include "chrome/browser/extensions/api/dial/dial_api_factory.h"
#include "chrome/browser/extensions/api/dial/dial_registry.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"

using extensions::DialDeviceData;
using extensions::Extension;

namespace api = extensions::api;

namespace {

class DialAPITest : public ExtensionApiTest {
 public:
  DialAPITest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }
};

}  // namespace

// Test receiving DIAL API events.
// Disabled due to http://crbug.com/167787
IN_PROC_BROWSER_TEST_F(DialAPITest, DISABLED_DeviceEvents) {
  scoped_refptr<extensions::DialAPI> api =
      extensions::DialAPIFactory::GetInstance()->GetForProfile(profile());
  ASSERT_TRUE(api.get());

  // Make sure we get device data.
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ExtensionTestMessageListener listener("ready", true);
  ExtensionTestMessageListener listener2("setup", true);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("dial/experimental")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("setup");
  listener2.WaitUntilSatisfied();

  extensions::DialRegistry::DeviceList devices;

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

  listener2.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test discoverNow fails if there are no listeners. When there are no listeners
// the DIAL API will not be active.
IN_PROC_BROWSER_TEST_F(DialAPITest, Discovery) {
  ASSERT_TRUE(RunExtensionSubtest("dial/experimental", "discovery.html"));
}

// Make sure this API is only accessible to whitelisted extensions.
IN_PROC_BROWSER_TEST_F(DialAPITest, NonWhitelistedExtension) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ExtensionTestMessageListener listener("ready", true);
  const extensions::Extension* extension = LoadExtensionWithFlags(
      test_data_dir_.AppendASCII("dial/whitelist"),
      ExtensionBrowserTest::kFlagIgnoreManifestWarnings);
  // We should have a DIAL API not available warning.
  ASSERT_FALSE(extension->install_warnings().empty());

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
