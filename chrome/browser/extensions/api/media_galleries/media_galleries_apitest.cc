// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
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
#include "chrome/common/chrome_paths.h"
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
  virtual void SetUpOnMainThread() OVERRIDE {
    PlatformAppBrowserTest::SetUpOnMainThread();
    ensure_media_directories_exist_.reset(new EnsureMediaDirectoriesExists);
    PopulatePicturesDirectoryTestData();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ensure_media_directories_exist_.reset();
    PlatformAppBrowserTest::TearDownOnMainThread();
  }

  // Since ExtensionTestMessageListener does not work with RunPlatformAppTest(),
  // This helper method can be used to run additional media gallery tests.
  void RunSecondTestPhase(const std::string& command) {
    const extensions::Extension* extension = GetSingleLoadedExtension();
    extensions::ExtensionHost* host =
        extensions::ExtensionSystem::Get(browser()->profile())->
            process_manager()->GetBackgroundHostForExtension(extension->id());
    ASSERT_TRUE(host);

    ResultCatcher catcher;
    host->render_view_host()->ExecuteJavascriptInWebFrame(
        base::string16(), base::UTF8ToUTF16(command));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  void AttachFakeDevice() {
    device_id_ = StorageInfo::MakeDeviceId(
        StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM, kDeviceId);

    StorageMonitor::GetInstance()->receiver()->ProcessAttach(
        StorageInfo(device_id_, base::string16(), kDevicePath,
                    ASCIIToUTF16(kDeviceName), base::string16(),
                    base::string16(), 0));
    content::RunAllPendingInMessageLoop();
  }

  void DetachFakeDevice() {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(device_id_);
    content::RunAllPendingInMessageLoop();
  }

  void PopulatePicturesDirectoryTestData() {
    if (ensure_media_directories_exist_->num_galleries() == 0)
      return;

    base::FilePath test_data_path =
        test_data_dir_.AppendASCII("api_test")
                      .AppendASCII("media_galleries")
                      .AppendASCII("common");
    base::FilePath write_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_USER_PICTURES, &write_path));

    // Valid file, should show up in JS as a FileEntry.
    ASSERT_TRUE(base::CopyFile(test_data_path.AppendASCII("test.jpg"),
                               write_path.AppendASCII("test.jpg")));

    // Invalid file, should not show up as a FileEntry in JS at all.
    ASSERT_TRUE(base::CopyFile(test_data_path.AppendASCII("test.txt"),
                               write_path.AppendASCII("test.txt")));
  }

  int num_galleries() const {
    return ensure_media_directories_exist_->num_galleries();
  }

 private:
  std::string device_id_;
  scoped_ptr<EnsureMediaDirectoriesExists> ensure_media_directories_exist_;
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesNoAccess) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_access"))
      << message_;
  RunSecondTestPhase(base::StringPrintf(kTestGalleries, num_galleries()));
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest, NoGalleriesRead) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_galleries"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       NoGalleriesCopyTo) {
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/media_galleries/no_galleries_copy_to")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesRead) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/read_access"))
      << message_;
  RunSecondTestPhase(base::StringPrintf(kTestGalleries, num_galleries()));
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesCopyTo) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MakeFakeMediaGalleryForTest(browser()->profile(), temp_dir.path());
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/copy_to_access"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesCopyToNoAccess) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MakeFakeMediaGalleryForTest(browser()->profile(), temp_dir.path());
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/media_galleries/copy_to_access/no_access"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesAccessAttached) {
  AttachFakeDevice();

  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/access_attached"))
      << message_;

  RunSecondTestPhase(base::StringPrintf(
      "testGalleries(%d, \"%s\")", num_galleries() + 1, kDeviceName));

  DetachFakeDevice();
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       GetFilesystemMetadata) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/metadata"))
      << message_;
}
