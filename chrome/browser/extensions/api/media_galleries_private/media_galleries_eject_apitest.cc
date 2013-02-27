// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MediaGalleriesPrivate eject API browser tests.

#include "base/files/file_path.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/media_gallery/media_galleries_test_util.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/storage_monitor/test_storage_monitor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"

namespace {

// Id of test extension from
// chrome/test/data/extensions/api_test/|kTestExtensionPath|
const char kTestExtensionId[] = "omeaapacpflbppbhigoacmagclbjhhmo";
const char kTestExtensionPath[] = "media_galleries_private/eject";

// JS commands.
const char kAddAttachListenerCmd[] = "addAttachListener()";
const char kRemoveAttachListenerCmd[] = "removeAttachListener()";
const char kEjectTestCmd[] = "ejectTest()";
const char kEjectFailTestCmd[] = "ejectFailTest()";

// And JS reply messages.
const char kAddAttachListenerOk[] = "add_attach_ok";
const char kAttachTestOk[] = "attach_test_ok";
const char kRemoveAttachListenerOk[] = "remove_attach_ok";
const char kEjectListenerOk[] = "eject_ok";
const char kEjectFailListenerOk[] = "eject_no_such_device";

const char kDeviceId[] = "testDeviceId";
const char kDeviceName[] = "foobar";
base::FilePath::CharType kDevicePath[] = FILE_PATH_LITERAL("/qux");

}  // namespace


///////////////////////////////////////////////////////////////////////////////
//                 MediaGalleriesPrivateEjectApiTest                         //
///////////////////////////////////////////////////////////////////////////////

class MediaGalleriesPrivateEjectApiTest : public ExtensionApiTest {
 public:
  MediaGalleriesPrivateEjectApiTest() {}
  virtual ~MediaGalleriesPrivateEjectApiTest() {}

 protected:
  // ExtensionApiTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kWhitelistedExtensionID,
                                    kTestExtensionId);
  }

  content::RenderViewHost* GetHost() {
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII(kTestExtensionPath));
    return extensions::ExtensionSystem::Get(browser()->profile())->
        process_manager()->GetBackgroundHostForExtension(extension->id())->
            render_view_host();
  }

  void ExecuteCmdAndCheckReply(content::RenderViewHost* host,
                               const std::string& js_command,
                               const std::string& ok_message) {
    ExtensionTestMessageListener listener(ok_message, false);
    host->ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(js_command));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  void Attach() {
    std::string device_id = chrome::MediaStorageUtil::MakeDeviceId(
        chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, kDeviceId);
    chrome::StorageMonitor::GetInstance()->receiver()->
        ProcessAttach(device_id, ASCIIToUTF16(kDeviceName), kDevicePath);
    content::RunAllPendingInMessageLoop();
  }

  void Detach() {
    std::string device_id = chrome::MediaStorageUtil::MakeDeviceId(
        chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, kDeviceId);
    chrome::StorageMonitor::GetInstance()->receiver()->
        ProcessDetach(device_id);
    content::RunAllPendingInMessageLoop();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPrivateEjectApiTest);
};


///////////////////////////////////////////////////////////////////////////////
//                               TESTS                                       //
///////////////////////////////////////////////////////////////////////////////

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateEjectApiTest, EjectTest) {
  scoped_ptr<chrome::test::TestStorageMonitor> monitor(
      chrome::test::TestStorageMonitor::CreateForBrowserTests());

  content::RenderViewHost* host = GetHost();
  ExecuteCmdAndCheckReply(host, kAddAttachListenerCmd, kAddAttachListenerOk);

  // Attach / detach
  const std::string expect_attach_msg =
      base::StringPrintf("%s,%s", kAttachTestOk, kDeviceName);
  ExtensionTestMessageListener attach_finished_listener(expect_attach_msg,
                                                        false  /* no reply */);
  Attach();
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());

  ExecuteCmdAndCheckReply(host, kEjectTestCmd, kEjectListenerOk);
  std::string device_id = chrome::MediaStorageUtil::MakeDeviceId(
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, kDeviceId);
  EXPECT_EQ(device_id, monitor->ejected_device());

  Detach();
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateEjectApiTest, EjectBadDeviceTest) {
  scoped_ptr<chrome::test::TestStorageMonitor> monitor(
      chrome::test::TestStorageMonitor::CreateForBrowserTests());

  ExecuteCmdAndCheckReply(GetHost(), kEjectFailTestCmd, kEjectFailListenerOk);

  EXPECT_EQ("", monitor->ejected_device());
}
