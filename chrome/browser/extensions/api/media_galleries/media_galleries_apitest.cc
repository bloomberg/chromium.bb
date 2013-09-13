// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

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

}  // namespace

// This function is to ensure at least one (fake) media gallery exists for
// testing platforms with no default media galleries, such as CHROMEOS.
void MakeFakeMediaGalleryForTest(Profile* profile, const base::FilePath& path) {
  base::RunLoop runloop;
  StorageMonitor::GetInstance()->EnsureInitialized(runloop.QuitClosure());
  runloop.Run();

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);

  MediaGalleryPrefInfo gallery_info;
  ASSERT_FALSE(preferences->LookUpGalleryByPath(path, &gallery_info));
  preferences->AddGallery(gallery_info.device_id,
                          gallery_info.path,
                          false /* user_added */,
                          gallery_info.volume_label,
                          gallery_info.vendor_name,
                          gallery_info.model_name,
                          gallery_info.total_size_in_bytes,
                          gallery_info.last_attach_time);

  content::RunAllPendingInMessageLoop();
}

class MediaGalleriesPlatformAppBrowserTest : public PlatformAppBrowserTest {
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
    device_id_ = StorageInfo::MakeDeviceId(
        StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM, kDeviceId);

    StorageMonitor::GetInstance()->receiver()->ProcessAttach(
        StorageInfo(device_id_, string16(), kDevicePath,
                    ASCIIToUTF16(kDeviceName), string16(), string16(), 0));
    content::RunAllPendingInMessageLoop();
  }

  void DetachFakeDevice() {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(device_id_);
    content::RunAllPendingInMessageLoop();
  }

 private:
  std::string device_id_;
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesNoAccess) {
  EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_access"))
      << message_;
  RunSecondTestPhase(base::UTF8ToUTF16(base::StringPrintf(
      kTestGalleries, media_directories.num_galleries())));
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest, NoGalleriesRead) {
  EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_galleries"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       NoGalleriesCopyTo) {
  EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/media_galleries/no_galleries_copy_to")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesRead) {
  EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/read_access"))
      << message_;
  RunSecondTestPhase(base::UTF8ToUTF16(base::StringPrintf(
      kTestGalleries, media_directories.num_galleries())));
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesCopyTo) {
  EnsureMediaDirectoriesExists media_directories;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MakeFakeMediaGalleryForTest(browser()->profile(), temp_dir.path());
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/copy_to_access"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesCopyToNoAccess) {
  EnsureMediaDirectoriesExists media_directories;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MakeFakeMediaGalleryForTest(browser()->profile(), temp_dir.path());
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/media_galleries/copy_to_access/no_access"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesAccessAttached) {
  EnsureMediaDirectoriesExists media_directories;

  AttachFakeDevice();

  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/access_attached"))
      << message_;

  RunSecondTestPhase(ASCIIToUTF16(base::StringPrintf(
      "testGalleries(%d, \"%s\")",
      media_directories.num_galleries() + 1, kDeviceName)));

  DetachFakeDevice();
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       GetFilesystemMetadata) {
  EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/metadata"))
      << message_;
}
