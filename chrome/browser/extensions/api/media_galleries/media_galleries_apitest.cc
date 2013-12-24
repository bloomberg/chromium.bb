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

}  // namespace

class MediaGalleriesPlatformAppBrowserTest : public PlatformAppBrowserTest {
 protected:
  MediaGalleriesPlatformAppBrowserTest() : test_jpg_size_(0) {}
  virtual ~MediaGalleriesPlatformAppBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    PlatformAppBrowserTest::SetUpOnMainThread();
    ensure_media_directories_exists_.reset(new EnsureMediaDirectoriesExists);

    int64 file_size;
    ASSERT_TRUE(base::GetFileSize(GetCommonDataDir().AppendASCII("test.jpg"),
                                  &file_size));
    test_jpg_size_ = base::checked_numeric_cast<int>(file_size);
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

  // Called if test only wants a single gallery it creates.
  void RemoveAllGalleries() {
    MediaGalleriesPreferences* preferences = GetAndInitializePreferences();

    // Make a copy, as the iterator would be invalidated otherwise.
    const MediaGalleriesPrefInfoMap galleries =
        preferences->known_galleries();
    for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
         it != galleries.end(); ++it) {
      preferences->ForgetGalleryById(it->first);
    }
  }

  // This function makes a single fake gallery. This is needed to test platforms
  // with no default media galleries, such as CHROMEOS. This fake gallery is
  // pre-populated with a test.jpg and test.txt.
  void MakeSingleFakeGallery() {
    ASSERT_TRUE(fake_gallery_temp_dir_.CreateUniqueTempDir());

    MediaGalleriesPreferences* preferences = GetAndInitializePreferences();

    MediaGalleryPrefInfo gallery_info;
    ASSERT_FALSE(preferences->LookUpGalleryByPath(fake_gallery_temp_dir_.path(),
                                                  &gallery_info));
    preferences->AddGallery(gallery_info.device_id,
                            gallery_info.path,
                            false /* user_added */,
                            gallery_info.volume_label,
                            gallery_info.vendor_name,
                            gallery_info.model_name,
                            gallery_info.total_size_in_bytes,
                            gallery_info.last_attach_time);

    content::RunAllPendingInMessageLoop();

    base::FilePath test_data_path(GetCommonDataDir());
    base::FilePath write_path = fake_gallery_temp_dir_.path();

    // Valid file, should show up in JS as a FileEntry.
    ASSERT_TRUE(base::CopyFile(test_data_path.AppendASCII("test.jpg"),
                               write_path.AppendASCII("test.jpg")));

    // Invalid file, should not show up as a FileEntry in JS at all.
    ASSERT_TRUE(base::CopyFile(test_data_path.AppendASCII("test.txt"),
                               write_path.AppendASCII("test.txt")));
  }

#if defined(OS_WIN) || defined(OS_MACOSX)
  void PopulatePicasaTestData(const base::FilePath& picasa_app_data_root) {
    base::FilePath picasa_database_path =
        picasa::MakePicasaDatabasePath(picasa_app_data_root);
    base::FilePath picasa_temp_dir_path =
        picasa_database_path.DirName().AppendASCII(picasa::kPicasaTempDirName);
    ASSERT_TRUE(base::CreateDirectory(picasa_database_path));
    ASSERT_TRUE(base::CreateDirectory(picasa_temp_dir_path));

    // Create fake folder directories.
    base::FilePath folders_root =
        ensure_media_directories_exists_->GetFakePicasaFoldersRootPath();
    base::FilePath fake_folder_1 = folders_root.AppendASCII("folder1");
    base::FilePath fake_folder_2 = folders_root.AppendASCII("folder2");
    ASSERT_TRUE(base::CreateDirectory(fake_folder_1));
    ASSERT_TRUE(base::CreateDirectory(fake_folder_2));

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
  MediaGalleriesPreferences* GetAndInitializePreferences() {
    MediaGalleriesPreferences* preferences =
        g_browser_process->media_file_system_registry()->GetPreferences(
            browser()->profile());
    base::RunLoop runloop;
    preferences->EnsureInitialized(runloop.QuitClosure());
    runloop.Run();
    return preferences;
  }

  std::string device_id_;
  base::ScopedTempDir fake_gallery_temp_dir_;
  int test_jpg_size_;
  scoped_ptr<EnsureMediaDirectoriesExists> ensure_media_directories_exists_;
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesNoAccess) {
  MakeSingleFakeGallery();

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
  RemoveAllGalleries();
  MakeSingleFakeGallery();
  base::ListValue custom_args;
  custom_args.AppendInteger(test_jpg_size());

  ASSERT_TRUE(RunMediaGalleriesTestWithArg("read_access", custom_args))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesCopyTo) {
  RemoveAllGalleries();
  MakeSingleFakeGallery();
  ASSERT_TRUE(RunMediaGalleriesTest("copy_to_access")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesDelete) {
  MakeSingleFakeGallery();
  base::ListValue custom_args;
  custom_args.AppendInteger(num_galleries() + 1);
  ASSERT_TRUE(RunMediaGalleriesTestWithArg("delete_access", custom_args))
      << message_;
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

  base::ListValue custom_args;
  custom_args.AppendInteger(test_jpg_size());
  ASSERT_TRUE(RunMediaGalleriesTestWithArg("picasa", custom_args)) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       PicasaCustomLocation) {
  base::ScopedTempDir custom_picasa_app_data_root;
  ASSERT_TRUE(custom_picasa_app_data_root.CreateUniqueTempDir());
  ensure_media_directories_exists()->SetCustomPicasaAppDataPath(
      custom_picasa_app_data_root.path());
  PopulatePicasaTestData(custom_picasa_app_data_root.path());

  base::ListValue custom_args;
  custom_args.AppendInteger(test_jpg_size());
  ASSERT_TRUE(RunMediaGalleriesTestWithArg("picasa", custom_args)) << message_;
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
