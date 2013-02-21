// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/removable_storage_notifications.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"

namespace {

// Id of test extension from
// chrome/test/data/extensions/api_test/|kTestExtensionPath|
const char kTestExtensionId[] = "lkegdcleigedmkiikoijjgfchobofdbe";
const char kTestExtensionPath[] = "media_galleries_private/attachdetach";

// JS commands.
const char kAddAttachListenerCmd[] = "addAttachListener()";
const char kAddDetachListenerCmd[] = "addDetachListener()";
const char kAddDummyDetachListenerCmd[] = "addDummyDetachListener()";
const char kRemoveAttachListenerCmd[] = "removeAttachListener()";
const char kRemoveDummyDetachListenerCmd[] = "removeDummyDetachListener()";

// And JS reply messages.
const char kAddAttachListenerOk[] = "add_attach_ok";
const char kAddDetachListenerOk[] = "add_detach_ok";
const char kAddDummyDetachListenerOk[] = "add_dummy_detach_ok";
const char kRemoveAttachListenerOk[] = "remove_attach_ok";
const char kRemoveDummyDetachListenerOk[] = "remove_dummy_detach_ok";

// Test reply messages.
const char kAttachTestOk[] = "attach_test_ok";
const char kDetachTestOk[] = "detach_test_ok";

// Dummy device properties.
const char kDeviceId[] = "testDeviceId";
const char kDeviceName[] = "foobar";
base::FilePath::CharType kDevicePath[] = FILE_PATH_LITERAL("/qux");

}  // namespace

class MediaGalleriesPrivateApiTest : public ExtensionApiTest {
 public:
  MediaGalleriesPrivateApiTest() {}
  virtual ~MediaGalleriesPrivateApiTest() {}

  // ExtensionApiTest overrides.
  virtual void SetUp() OVERRIDE {
    device_id_ = chrome::MediaStorageUtil::MakeDeviceId(
        chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, kDeviceId);
    ExtensionApiTest::SetUp();
  }

 protected:
  // ExtensionApiTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kWhitelistedExtensionID,
                                    kTestExtensionId);
  }

  void ChangeListener(content::RenderViewHost* host,
                      const std::string& js_command,
                      const std::string& ok_message) {
    ExtensionTestMessageListener listener(ok_message, false  /* no reply */);
    host->ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(js_command));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  void AttachDetach() {
    Attach();
    Detach();
  }

  void Attach() {
    chrome::RemovableStorageNotifications::GetInstance()->receiver()->
        ProcessAttach(device_id_, ASCIIToUTF16(kDeviceName), kDevicePath);
    WaitForDeviceEvents();
  }

  void Detach() {
    chrome::RemovableStorageNotifications::GetInstance()->receiver()->
        ProcessDetach(device_id_);
    WaitForDeviceEvents();
  }

 private:
  void WaitForDeviceEvents() {
    content::RunAllPendingInMessageLoop();
  }

  std::string device_id_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateApiTest, DeviceAttachDetachEvents) {
  // Setup.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kTestExtensionPath));
  ASSERT_TRUE(extension);

  content::RenderViewHost* host =
      extensions::ExtensionSystem::Get(browser()->profile())->
          process_manager()->GetBackgroundHostForExtension(extension->id())->
              render_view_host();
  ASSERT_TRUE(host);

  // No listeners, attach and detach a couple times.
  AttachDetach();
  AttachDetach();

  // Add attach listener.
  ChangeListener(host, kAddAttachListenerCmd, kAddAttachListenerOk);

  // Attach / detach
  const std::string expect_attach_msg =
      base::StringPrintf("%s,%s", kAttachTestOk, kDeviceName);
  ExtensionTestMessageListener attach_finished_listener(expect_attach_msg,
                                                        false  /* no reply */);
  Attach();
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());
  Detach();

  // Attach / detach
  Attach();
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());
  // Detach
  Detach();

  // Remove attach listener.
  ChangeListener(host, kRemoveAttachListenerCmd, kRemoveAttachListenerOk);

  // No listeners, attach and detach a couple times.
  AttachDetach();
  AttachDetach();

  // Add detach listener.
  ChangeListener(host, kAddDummyDetachListenerCmd, kAddDummyDetachListenerOk);

  // Attach / detach
  Attach();

  ExtensionTestMessageListener detach_finished_listener(kDetachTestOk,
                                                        false  /* no reply */);
  Detach();
  EXPECT_TRUE(detach_finished_listener.WaitUntilSatisfied());

  // Attach / detach
  Attach();
  Detach();
  EXPECT_TRUE(detach_finished_listener.WaitUntilSatisfied());

  // Switch ok dummy detach listener for the regular one.
  ChangeListener(host, kRemoveDummyDetachListenerCmd,
                 kRemoveDummyDetachListenerOk);
  ChangeListener(host, kAddDetachListenerCmd, kAddDetachListenerOk);

  // Add attach listener.
  ChangeListener(host, kAddAttachListenerCmd, kAddAttachListenerOk);

  Attach();
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());
  Detach();
  EXPECT_TRUE(detach_finished_listener.WaitUntilSatisfied());

  Attach();
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());
  Detach();
  EXPECT_TRUE(detach_finished_listener.WaitUntilSatisfied());
}
