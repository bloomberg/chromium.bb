// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"

using extensions::PlatformAppBrowserTest;

namespace {

// Dummy device properties.
const char kDeviceId[] = "testDeviceId";
const char kDeviceName[] = "foobar";
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
base::FilePath::CharType kDevicePath[] = FILE_PATH_LITERAL("C:\\qux");
#else
base::FilePath::CharType kDevicePath[] = FILE_PATH_LITERAL("/qux");
#endif

const char kTestGalleries[] = "testGalleries(%d)";

class ExperimentalMediaGalleriesApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

}  // namespace

class PlatformAppMediaGalleriesBrowserTest : public PlatformAppBrowserTest {
 protected:
  // Since ExtensionTestMessageListener does not work with RunPlatformAppTest(),
  // This helper method can be used to run additional media gallery tests.
  void RunSecondTestPhase(const string16& command) {
    const extensions::Extension* extension = GetSingleLoadedExtension();
    extensions::ExtensionHost* host =
        extensions::ExtensionSystem::Get(browser()->profile())->
            process_manager()->GetBackgroundHostForExtension(extension->id());
    ASSERT_TRUE(host);

    ResultCatcher catcher;
    host->render_view_host()->ExecuteJavascriptInWebFrame(string16(), command);
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  void AttachFakeDevice() {
    device_id_ = chrome::MediaStorageUtil::MakeDeviceId(
        chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, kDeviceId);

    chrome::StorageMonitor::GetInstance()->receiver()->ProcessAttach(
        chrome::StorageInfo(device_id_, ASCIIToUTF16(kDeviceName), kDevicePath,
                            string16(), string16(), string16(), 0));
    content::RunAllPendingInMessageLoop();
  }

  void DetachFakeDevice() {
    chrome::StorageMonitor::GetInstance()->receiver()->ProcessDetach(
        device_id_);
    content::RunAllPendingInMessageLoop();
  }

 private:
  std::string device_id_;
};

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest,
                       MediaGalleriesNoAccess) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_access"))
      << message_;
  RunSecondTestPhase(UTF8ToUTF16(base::StringPrintf(
      kTestGalleries, media_directories.num_galleries())));
}

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest, NoGalleriesRead) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_galleries"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest,
                       MediaGalleriesRead) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/read_access"))
      << message_;
  RunSecondTestPhase(UTF8ToUTF16(base::StringPrintf(
      kTestGalleries, media_directories.num_galleries())));
}

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest,
                       MediaGalleriesAccessAttached) {
  chrome::EnsureMediaDirectoriesExists media_directories;

  AttachFakeDevice();

  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/access_attached"))
      << message_;

  RunSecondTestPhase(ASCIIToUTF16(base::StringPrintf(
      "testGalleries(%d, \"%s\")",
      media_directories.num_galleries() + 1, kDeviceName)));

  DetachFakeDevice();
}

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest,
                       GetFilesystemMetadata) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/metadata"))
      << message_;
}


IN_PROC_BROWSER_TEST_F(ExperimentalMediaGalleriesApiTest,
                       ExperimentalMediaGalleries) {
  ASSERT_TRUE(RunExtensionTest("media_galleries/experimental")) << message_;
}
