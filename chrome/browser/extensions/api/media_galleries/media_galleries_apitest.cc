// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
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

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/media_galleries/fileapi/picasa_finder.h"
#include "chrome/common/media_galleries/picasa_test_util.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "chrome/common/media_galleries/pmp_test_util.h"
#endif

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

// This function is to ensure at least one (fake) media gallery exists for
// testing platforms with no default media galleries, such as CHROMEOS.
void MakeFakeMediaGalleryForTest(Profile* profile, const base::FilePath& path) {
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);
  base::RunLoop runloop;
  preferences->EnsureInitialized(runloop.QuitClosure());
  runloop.Run();

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

}  // namespace

class MediaGalleriesPlatformAppBrowserTest : public PlatformAppBrowserTest {
 protected:
  MediaGalleriesPlatformAppBrowserTest() : test_jpg_size_(0) {}
  virtual ~MediaGalleriesPlatformAppBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    PlatformAppBrowserTest::SetUpOnMainThread();
    ensure_media_directories_exists_.reset(new EnsureMediaDirectoriesExists);
    PopulatePicturesDirectoryTestData();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ensure_media_directories_exists_.reset();
    PlatformAppBrowserTest::TearDownOnMainThread();
  }

  bool RunMediaGalleriesTest(const std::string& extension_name) {
    base::ListValue empty_list_value;
    return RunMediaGalleriesTestWithArg(extension_name, empty_list_value);
  }

  bool RunMediaGalleriesTestWithArg(const std::string& extension_name,
                                    const base::ListValue& custom_arg_value) {
    // Copy the test data for this test into a temporary directory. Then add
    // a common_injected.js to the temporary copy and run it.
    const char kTestDir[] = "api_test/media_galleries/";
    base::FilePath from_dir =
        test_data_dir_.AppendASCII(kTestDir + extension_name);
    from_dir = from_dir.NormalizePathSeparators();

    base::ScopedTempDir temp_dir;
    if (!temp_dir.CreateUniqueTempDir())
      return false;

    if (!base::CopyDirectory(from_dir, temp_dir.path(), true))
      return false;

    base::FilePath common_js_path(
        GetCommonDataDir().AppendASCII("common_injected.js"));
    base::FilePath inject_js_path(
        temp_dir.path().AppendASCII(extension_name)
                       .AppendASCII("common_injected.js"));
    if (!base::CopyFile(common_js_path, inject_js_path))
      return false;

    const char* custom_arg = NULL;
    std::string json_string;
    if (!custom_arg_value.empty()) {
      base::JSONWriter::Write(&custom_arg_value, &json_string);
      custom_arg = json_string.c_str();
    }

    base::AutoReset<base::FilePath> reset(&test_data_dir_, temp_dir.path());
    return RunPlatformAppTestWithArg(extension_name, custom_arg);
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
    if (ensure_media_directories_exists_->num_galleries() == 0)
      return;

    base::FilePath test_data_path(GetCommonDataDir());
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

#if defined(OS_WIN) || defined(OS_MACOSX)
  void PopulatePicasaTestData(const base::FilePath& picasa_app_data_root) {
    base::FilePath picasa_database_path =
        picasa::MakePicasaDatabasePath(picasa_app_data_root);
    base::FilePath picasa_temp_dir_path =
        picasa_database_path.DirName().AppendASCII(picasa::kPicasaTempDirName);
    ASSERT_TRUE(file_util::CreateDirectory(picasa_database_path));
    ASSERT_TRUE(file_util::CreateDirectory(picasa_temp_dir_path));

    // Create fake folder directories.
    base::FilePath folders_root =
        ensure_media_directories_exists_->GetFakePicasaFoldersRootPath();
    base::FilePath fake_folder_1 = folders_root.AppendASCII("folder1");
    base::FilePath fake_folder_2 = folders_root.AppendASCII("folder2");
    ASSERT_TRUE(file_util::CreateDirectory(fake_folder_1));
    ASSERT_TRUE(file_util::CreateDirectory(fake_folder_2));

    // Write folder and album contents.
    picasa::WriteTestAlbumTable(
        picasa_database_path, fake_folder_1, fake_folder_2);
    picasa::WriteTestAlbumsImagesIndex(fake_folder_1, fake_folder_2);

    base::FilePath test_jpg_path = GetCommonDataDir().AppendASCII("test.jpg");
    ASSERT_TRUE(base::CopyFile(
        test_jpg_path, fake_folder_1.AppendASCII("InBoth.jpg")));
    ASSERT_TRUE(base::CopyFile(
        test_jpg_path, fake_folder_1.AppendASCII("InSecondAlbumOnly.jpg")));
    ASSERT_TRUE(base::CopyFile(
        test_jpg_path, fake_folder_2.AppendASCII("InFirstAlbumOnly.jpg")));
  }
#endif

  base::FilePath GetCommonDataDir() const {
    return test_data_dir_.AppendASCII("api_test")
                         .AppendASCII("media_galleries")
                         .AppendASCII("common");
  }

  int num_galleries() const {
    return ensure_media_directories_exists_->num_galleries();
  }

  int test_jpg_size() const { return test_jpg_size_; }

  EnsureMediaDirectoriesExists* ensure_media_directories_exists() const {
    return ensure_media_directories_exists_.get();
  }

 private:
  std::string device_id_;
  int test_jpg_size_;
  scoped_ptr<EnsureMediaDirectoriesExists> ensure_media_directories_exists_;
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesNoAccess) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MakeFakeMediaGalleryForTest(browser()->profile(), temp_dir.path());

  base::ListValue custom_args;
  custom_args.AppendInteger(num_galleries() + 1);

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

// Flaky: crbug.com/314576
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       DISABLED_MediaGalleriesCopyTo) {
#else
IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesCopyTo) {
#endif
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MakeFakeMediaGalleryForTest(browser()->profile(), temp_dir.path());
  ASSERT_TRUE(RunMediaGalleriesTest("copy_to_access")) << message_;
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

#if defined(OS_WIN)|| defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       PicasaDefaultLocation) {
#if defined(OS_WIN)
  PopulatePicasaTestData(
      ensure_media_directories_exists()->GetFakeLocalAppDataPath());
#elif defined(OS_MACOSX)
  PopulatePicasaTestData(
      ensure_media_directories_exists()->GetFakeAppDataPath());
#endif
  ASSERT_TRUE(RunMediaGalleriesTest("picasa")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       PicasaCustomLocation) {
  base::ScopedTempDir custom_picasa_app_data_root;
  ASSERT_TRUE(custom_picasa_app_data_root.CreateUniqueTempDir());
  ensure_media_directories_exists()->SetCustomPicasaAppDataPath(
      custom_picasa_app_data_root.path());
  PopulatePicasaTestData(custom_picasa_app_data_root.path());
  ASSERT_TRUE(RunMediaGalleriesTest("picasa")) << message_;
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
