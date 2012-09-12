// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"

namespace {

// Id of test extension from
// chrome/test/data/extensions/api_test/|kTestExtensionPath|
const char kTestExtensionId[] = "lkegdcleigedmkiikoijjgfchobofdbe";
const char kTestExtensionPath[] = "media_galleries_private/attachdetach";

const char kAttachTestOk[] = "attach_test_ok";
const char kDetachTestOk[] = "detach_test_ok";

const char kDeviceId[] = "testDeviceId";
const char kDeviceName[] = "foobar";
FilePath::CharType kDevicePath[] = FILE_PATH_LITERAL("/qux");

}  // namespace

class MediaGalleriesPrivateApiTest : public ExtensionApiTest {
 protected:
  // ExtensionApiTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kWhitelistedExtensionID,
                                    kTestExtensionId);
  }
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateApiTest, DeviceAttachDetachEvents) {
  // Setup.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kTestExtensionPath));
  ASSERT_TRUE(extension);

  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  ASSERT_TRUE(system_monitor);

  const std::string device_id = chrome::MediaStorageUtil::MakeDeviceId(
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, kDeviceId);

  // Attach event.
  const std::string expect_attach_msg =
      base::StringPrintf("%s,%s", kAttachTestOk, kDeviceName);
  ExtensionTestMessageListener attach_finished_listener(expect_attach_msg,
                                                        false  /* no reply */);
  system_monitor->ProcessRemovableStorageAttached(
      device_id, ASCIIToUTF16(kDeviceName), kDevicePath);
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());

  // Detach event.
  ExtensionTestMessageListener detach_finished_listener(kDetachTestOk,
                                                        false  /* no reply */);
  system_monitor->ProcessRemovableStorageDetached(device_id);
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());
}
