// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/safe_numerics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/common/chrome_paths.h"
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
  MediaGalleriesPlatformAppBrowserTest() : test_jpg_size_(0) {}
  virtual ~MediaGalleriesPlatformAppBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    PlatformAppBrowserTest::SetUpOnMainThread();
    ensure_media_directories_exist_.reset(new EnsureMediaDirectoriesExists);
    PopulatePicturesDirectoryTestData();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ensure_media_directories_exist_.reset();
    PlatformAppBrowserTest::TearDownOnMainThread();
  }

  bool RunMediaGalleriesTest(const std::string& extension_name) {
    base::ListValue empty_list_value;
    return RunMediaGalleriesTestWithArg(extension_name, empty_list_value);
  }

  bool RunMediaGalleriesTestWithArg(const std::string& extension_name,
                                    const base::ListValue& custom_arg_value) {
    const char* custom_arg = NULL;
    std::string json_string;
    if (!custom_arg_value.empty()) {
      base::JSONWriter::Write(&custom_arg_value, &json_string);
      custom_arg = json_string.c_str();
    }

    const char kTestDir[] = "api_test/media_galleries/";
    return RunPlatformAppTestWithArg(kTestDir + extension_name, custom_arg);
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

    int64 file_size;
    ASSERT_TRUE(file_util::GetFileSize(test_data_path.AppendASCII("test.jpg"),
                                       &file_size));
    test_jpg_size_ = base::checked_numeric_cast<int>(file_size);
  }

  int num_galleries() const {
    return ensure_media_directories_exist_->num_galleries();
  }

  int test_jpg_size() const { return test_jpg_size_; }

 private:
  std::string device_id_;
  int test_jpg_size_;
  scoped_ptr<EnsureMediaDirectoriesExists> ensure_media_directories_exist_;
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesNoAccess) {
  base::ListValue custom_args;
  custom_args.AppendInteger(num_galleries());

  ASSERT_TRUE(RunMediaGalleriesTestWithArg("no_access", custom_args))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest, NoGalleriesRead) {
  ASSERT_TRUE(RunMediaGalleriesTest("no_galleries")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       NoGalleriesCopyTo) {
  ASSERT_TRUE(RunMediaGalleriesTest("no_galleries_copy_to")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesRead) {
  base::ListValue custom_args;
  custom_args.AppendInteger(num_galleries());
  custom_args.AppendInteger(test_jpg_size());

  ASSERT_TRUE(RunMediaGalleriesTestWithArg("read_access", custom_args))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesCopyTo) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MakeFakeMediaGalleryForTest(browser()->profile(), temp_dir.path());
  ASSERT_TRUE(RunMediaGalleriesTest("copy_to_access")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesCopyToNoAccess) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MakeFakeMediaGalleryForTest(browser()->profile(), temp_dir.path());
  ASSERT_TRUE(RunMediaGalleriesTest("copy_to_access/no_access")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesAccessAttached) {
  AttachFakeDevice();

  base::ListValue custom_args;
  custom_args.AppendInteger(num_galleries() + 1);
  custom_args.AppendString(kDeviceName);

  ASSERT_TRUE(RunMediaGalleriesTestWithArg("access_attached", custom_args))
      << message_;

  DetachFakeDevice();
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       GetFilesystemMetadata) {
  ASSERT_TRUE(RunMediaGalleriesTest("metadata")) << message_;
}
