// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_folder_finder.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/browser/media_galleries/media_scan_manager.h"
#include "chrome/browser/media_galleries/media_scan_manager_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/media_galleries_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace {

class MockMediaFolderFinder : MediaFolderFinder {
 public:
  typedef base::Callback<void(MediaFolderFinderResultsCallback)>
      FindFoldersStartedCallback;

  static MediaFolderFinder* CreateMockMediaFolderFinder(
      const FindFoldersStartedCallback& started_callback,
      const base::Closure destruction_callback,
      const MediaFolderFinderResultsCallback& callback) {
    return new MockMediaFolderFinder(started_callback, destruction_callback,
                                     callback);
  }

  MockMediaFolderFinder(
      const FindFoldersStartedCallback& started_callback,
      const base::Closure destruction_callback,
      const MediaFolderFinderResultsCallback& callback)
      : MediaFolderFinder(callback),
        started_callback_(started_callback),
        destruction_callback_(destruction_callback),
        callback_(callback) {
  }
  virtual ~MockMediaFolderFinder() {
    destruction_callback_.Run();
  }

  virtual void StartScan() OVERRIDE {
    started_callback_.Run(callback_);
  }

 private:
  FindFoldersStartedCallback started_callback_;
  base::Closure destruction_callback_;
  MediaFolderFinderResultsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaFolderFinder);
};

}  // namespace

class TestMediaScanManager : public MediaScanManager {
 public:
  typedef base::Callback<MediaFolderFinder*(
      const MediaFolderFinder::MediaFolderFinderResultsCallback&)>
          MediaFolderFinderFactory;

  explicit TestMediaScanManager(const MediaFolderFinderFactory& factory) {
    SetMediaFolderFinderFactory(factory);
  }
  virtual ~TestMediaScanManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMediaScanManager);
};

class MediaScanManagerTest : public MediaScanManagerObserver,
                             public testing::Test {
 public:
  MediaScanManagerTest()
      : find_folders_start_count_(0),
        find_folders_destroy_count_(0),
        find_folders_success_(false),
        expected_gallery_count_(0),
        profile_(new TestingProfile()) {}

  virtual ~MediaScanManagerTest() {
    EXPECT_EQ(find_folders_start_count_, find_folders_destroy_count_);
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(storage_monitor::TestStorageMonitor::CreateAndInstall());

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
    extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);

    gallery_prefs_ =
        MediaGalleriesPreferencesFactory::GetForProfile(profile_.get());
    base::RunLoop loop;
    gallery_prefs_->EnsureInitialized(loop.QuitClosure());
    loop.Run();

    std::vector<std::string> read_permissions;
    read_permissions.push_back(
        extensions::MediaGalleriesPermission::kReadPermission);
    extension_ = AddMediaGalleriesApp("read", read_permissions, profile_.get());

    ASSERT_TRUE(test_results_dir_.CreateUniqueTempDir());

    MockMediaFolderFinder::FindFoldersStartedCallback started_callback =
        base::Bind(&MediaScanManagerTest::OnFindFoldersStarted,
                   base::Unretained(this));
    base::Closure destruction_callback =
        base::Bind(&MediaScanManagerTest::OnFindFoldersDestroyed,
                   base::Unretained(this));
    TestMediaScanManager::MediaFolderFinderFactory factory =
        base::Bind(&MockMediaFolderFinder::CreateMockMediaFolderFinder,
                   started_callback, destruction_callback);
    media_scan_manager_.reset(new TestMediaScanManager(factory));
    media_scan_manager_->AddObserver(profile_.get(), this);
  }

  virtual void TearDown() OVERRIDE {
    media_scan_manager_->RemoveObserver(profile_.get());
    media_scan_manager_.reset();
    storage_monitor::TestStorageMonitor::Destroy();
  }

  // Create a test folder in the test specific scoped temp dir and return the
  // final path in |full_path|.
  void MakeTestFolder(const std::string& root_relative_path,
                      base::FilePath* full_path) {
    ASSERT_TRUE(test_results_dir_.IsValid());
    *full_path =
        test_results_dir_.path().AppendASCII(root_relative_path);
    ASSERT_TRUE(base::CreateDirectory(*full_path));
  }

  // Create the specified path, and add it to preferences as a gallery.
  MediaGalleryPrefId AddGallery(const std::string& rel_path,
                                MediaGalleryPrefInfo::Type type,
                                int audio_count,
                                int image_count,
                                int video_count) {
    base::FilePath full_path;
    MakeTestFolder(rel_path, &full_path);
    MediaGalleryPrefInfo gallery_info;
    gallery_prefs_->LookUpGalleryByPath(full_path, &gallery_info);
    return gallery_prefs_->AddGallery(gallery_info.device_id,
                                      gallery_info.path,
                                      type,
                                      gallery_info.volume_label,
                                      gallery_info.vendor_name,
                                      gallery_info.model_name,
                                      gallery_info.total_size_in_bytes,
                                      gallery_info.last_attach_time,
                                      audio_count, image_count, video_count);
  }

  void SetFindFoldersResults(
      bool success,
      const MediaFolderFinder::MediaFolderFinderResults& results) {
    find_folders_success_ = success;
    find_folders_results_ = results;
  }

  void SetExpectedScanResults(int gallery_count,
                               const MediaGalleryScanResult& file_counts) {
    expected_gallery_count_ = gallery_count;
    expected_file_counts_ = file_counts;
  }

  void StartScan() {
    media_scan_manager_->StartScan(profile_.get(), extension_,
                                   true /* user_gesture */);
  }

  MediaGalleriesPreferences* gallery_prefs() {
    return gallery_prefs_;
  }

  const MediaGalleriesPrefInfoMap& known_galleries() const {
    return gallery_prefs_->known_galleries();
  }

  size_t gallery_count() const {
    return known_galleries().size();
  }

  extensions::Extension* extension() {
    return extension_.get();
  }

  int FindFoldersStartCount() {
    return find_folders_start_count_;
  }

  int FindFolderDestroyCount() {
    return find_folders_destroy_count_;
  }

  void CheckFileCounts(MediaGalleryPrefId pref_id, int audio_count,
                       int image_count, int video_count) {
    if (!ContainsKey(known_galleries(), pref_id)) {
      EXPECT_TRUE(false);
      return;
    }
    MediaGalleriesPrefInfoMap::const_iterator pref_info =
        known_galleries().find(pref_id);
    EXPECT_EQ(audio_count, pref_info->second.audio_count);
    EXPECT_EQ(image_count, pref_info->second.image_count);
    EXPECT_EQ(video_count, pref_info->second.video_count);
  }

  // MediaScanManagerObserver implementation.
  virtual void OnScanFinished(
      const std::string& extension_id,
      int gallery_count,
      const MediaGalleryScanResult& file_counts) OVERRIDE {
    EXPECT_EQ(extension_->id(), extension_id);
    EXPECT_EQ(expected_gallery_count_, gallery_count);
    EXPECT_EQ(expected_file_counts_.audio_count, file_counts.audio_count);
    EXPECT_EQ(expected_file_counts_.image_count, file_counts.image_count);
    EXPECT_EQ(expected_file_counts_.video_count, file_counts.video_count);
  }

 protected:
  // So derived tests can access ...::FindContainerScanResults().
  MediaFolderFinder::MediaFolderFinderResults FindContainerScanResults(
      const MediaFolderFinder::MediaFolderFinderResults& found_folders,
      const std::vector<base::FilePath>& sensitive_locations) {
    return MediaScanManager::FindContainerScanResults(found_folders,
                                                      sensitive_locations);
  }

 private:
  void OnFindFoldersStarted(
      MediaFolderFinder::MediaFolderFinderResultsCallback callback) {
    find_folders_start_count_++;
    callback.Run(find_folders_success_, find_folders_results_);
  }

  void OnFindFoldersDestroyed() {
    find_folders_destroy_count_++;
  }

  scoped_ptr<TestMediaScanManager> media_scan_manager_;

  int find_folders_start_count_;
  int find_folders_destroy_count_;
  bool find_folders_success_;
  MediaFolderFinder::MediaFolderFinderResults find_folders_results_;

  int expected_gallery_count_;
  MediaGalleryScanResult expected_file_counts_;

  base::ScopedTempDir test_results_dir_;

  // Needed for extension service & friends to work.
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<extensions::Extension> extension_;

  EnsureMediaDirectoriesExists mock_gallery_locations_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  storage_monitor::TestStorageMonitor monitor_;
  scoped_ptr<TestingProfile> profile_;
  MediaGalleriesPreferences* gallery_prefs_;

  DISALLOW_COPY_AND_ASSIGN(MediaScanManagerTest);
};

TEST_F(MediaScanManagerTest, SingleResult) {
  size_t galleries_before = gallery_count();
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 1;
  file_counts.image_count = 2;
  file_counts.video_count = 3;
  base::FilePath path;
  MakeTestFolder("found_media_folder", &path);

  MediaFolderFinder::MediaFolderFinderResults found_folders;
  found_folders[path] = file_counts;
  SetFindFoldersResults(true, found_folders);

  SetExpectedScanResults(1 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 1, gallery_count());
}

// Generally test that it includes directories with sufficient density
// and excludes others.
//
// A/ - NOT included
// A/B/ - NOT included
// A/B/C/files
// A/D/ - NOT included
// A/D/E/files
// A/D/F/files
// A/D/G/files
// A/D/H/
// A/H/ - included in results
// A/H/I/files
// A/H/J/files
TEST_F(MediaScanManagerTest, MergeRedundant) {
  base::FilePath path;
  MediaFolderFinder::MediaFolderFinderResults found_folders;
  std::vector<base::FilePath> sensitive_locations;
  std::vector<base::FilePath> expected_folders;
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 1;
  file_counts.image_count = 2;
  file_counts.video_count = 3;
  MakeTestFolder("A", &path);
  MakeTestFolder("A/B", &path);
  MakeTestFolder("A/B/C", &path);
  found_folders[path] = file_counts;
  // Not dense enough.
  MakeTestFolder("A/D", &path);
  MakeTestFolder("A/D/E", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/D/F", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/D/G", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/D/H", &path);
  // Dense enough to be reported.
  MakeTestFolder("A/H", &path);
  expected_folders.push_back(path);
  MakeTestFolder("A/H/I", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/H/J", &path);
  found_folders[path] = file_counts;
  MediaFolderFinder::MediaFolderFinderResults results =
      FindContainerScanResults(found_folders, sensitive_locations);
  EXPECT_EQ(expected_folders.size(), results.size());
  for (std::vector<base::FilePath>::const_iterator it =
           expected_folders.begin();
       it != expected_folders.end();
       ++it) {
    EXPECT_TRUE(results.find(*it) != results.end());
  }
}

// Make sure intermediates are not included.
//
// A/ - included in results
// A/B1/ - NOT included
// A/B1/B2/files
// A/C1/ - NOT included
// A/C1/C2/files
TEST_F(MediaScanManagerTest, MergeRedundantNoIntermediates) {
  base::FilePath path;
  MediaFolderFinder::MediaFolderFinderResults found_folders;
  std::vector<base::FilePath> sensitive_locations;
  std::vector<base::FilePath> expected_folders;
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 1;
  file_counts.image_count = 2;
  file_counts.video_count = 3;
  MakeTestFolder("A", &path);
  expected_folders.push_back(path);
  MakeTestFolder("A/B1", &path);
  MakeTestFolder("A/B1/B2", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/C1", &path);
  MakeTestFolder("A/C1/C2", &path);
  found_folders[path] = file_counts;
  // Make "home dir" not dense enough.
  MakeTestFolder("D", &path);
  MediaFolderFinder::MediaFolderFinderResults results =
      FindContainerScanResults(found_folders, sensitive_locations);
  EXPECT_EQ(expected_folders.size(), results.size());
  for (std::vector<base::FilePath>::const_iterator it =
           expected_folders.begin();
       it != expected_folders.end();
       ++it) {
    EXPECT_TRUE(results.find(*it) != results.end());
  }
}

// Make sure "A/" only gets a count of 1, from "A/D/",
// not 2 from "A/D/H/" and "A/D/I/".
//
// A/ - NOT included
// A/D/ - included in results
// A/D/E/files
// A/D/F/files
// A/D/G/files
// A/D/H/files
// A/D/I/ - NOT included
// A/D/I/J/files
TEST_F(MediaScanManagerTest, MergeRedundantVerifyNoOvercount) {
  base::FilePath path;
  MediaFolderFinder::MediaFolderFinderResults found_folders;
  std::vector<base::FilePath> sensitive_locations;
  std::vector<base::FilePath> expected_folders;
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 1;
  file_counts.image_count = 2;
  file_counts.video_count = 3;
  MakeTestFolder("A", &path);
  MakeTestFolder("A/D", &path);
  expected_folders.push_back(path);
  MakeTestFolder("A/D/E", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/D/F", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/D/G", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/D/H", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/D/I", &path);
  MakeTestFolder("A/D/I/J", &path);
  found_folders[path] = file_counts;
  MediaFolderFinder::MediaFolderFinderResults results =
      FindContainerScanResults(found_folders, sensitive_locations);
  EXPECT_EQ(expected_folders.size(), results.size());
  for (std::vector<base::FilePath>::const_iterator it =
           expected_folders.begin();
       it != expected_folders.end();
       ++it) {
    EXPECT_TRUE(results.find(*it) != results.end());
  }
}

// Make sure that sensistive directories are pruned.
//
// A/ - NOT included
// A/B/ - sensitive
// A/C/ - included in results
// A/C/G/files
// A/C/H/files
// A/D/ - included in results
// A/D/I/files
// A/D/J/files
// A/E/ - included in results
// A/E/K/files
// A/E/L/files
// A/F/ - included in results
// A/F/M/files
// A/F/N/files
TEST_F(MediaScanManagerTest, MergeRedundantWithSensitive) {
  base::FilePath path;
  MediaFolderFinder::MediaFolderFinderResults found_folders;
  std::vector<base::FilePath> sensitive_locations;
  std::vector<base::FilePath> expected_folders;
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 1;
  file_counts.image_count = 2;
  file_counts.video_count = 3;
  MakeTestFolder("A", &path);
  MakeTestFolder("A/B", &path);
  sensitive_locations.push_back(path);
  MakeTestFolder("A/C", &path);
  expected_folders.push_back(path);
  MakeTestFolder("A/C/G", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/C/H", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/D", &path);
  expected_folders.push_back(path);
  MakeTestFolder("A/D/I", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/D/J", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/E", &path);
  expected_folders.push_back(path);
  MakeTestFolder("A/E/K", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/E/L", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/F", &path);
  expected_folders.push_back(path);
  MakeTestFolder("A/F/M", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("A/F/N", &path);
  found_folders[path] = file_counts;
  MediaFolderFinder::MediaFolderFinderResults results =
      FindContainerScanResults(found_folders, sensitive_locations);
  EXPECT_EQ(expected_folders.size(), results.size());
  for (std::vector<base::FilePath>::const_iterator it =
           expected_folders.begin();
       it != expected_folders.end();
       ++it) {
    EXPECT_TRUE(results.find(*it) != results.end());
  }
}

TEST_F(MediaScanManagerTest, Containers) {
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 1;
  base::FilePath path;
  std::set<base::FilePath> expected_galleries;
  std::set<base::FilePath> bad_galleries;
  MediaFolderFinder::MediaFolderFinderResults found_folders;
  size_t galleries_before = gallery_count();

  // Should manifest as a gallery in result1.
  MakeTestFolder("dir1/result1", &path);
  expected_galleries.insert(path);
  found_folders[path] = file_counts;

  // Should manifest as a gallery in dir2.
  MakeTestFolder("dir2/result2", &path);
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  MakeTestFolder("dir2/result3", &path);
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  expected_galleries.insert(path.DirName());

  // Should manifest as a two galleries: result4 and result5.
  MakeTestFolder("dir3/other", &path);
  bad_galleries.insert(path);
  MakeTestFolder("dir3/result4", &path);
  expected_galleries.insert(path);
  found_folders[path] = file_counts;
  MakeTestFolder("dir3/result5", &path);
  expected_galleries.insert(path);
  found_folders[path] = file_counts;

  // Should manifest as a gallery in dir4.
  MakeTestFolder("dir4/other", &path);
  bad_galleries.insert(path);
  MakeTestFolder("dir4/result6", &path);
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  MakeTestFolder("dir4/result7", &path);
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  MakeTestFolder("dir4/result8", &path);
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  MakeTestFolder("dir4/result9", &path);
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  expected_galleries.insert(path.DirName());

  SetFindFoldersResults(true, found_folders);

  file_counts.audio_count = 9;
  SetExpectedScanResults(5 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 5, gallery_count());

  std::set<base::FilePath> found_galleries;
  for (MediaGalleriesPrefInfoMap::const_iterator it = known_galleries().begin();
       it != known_galleries().end();
       ++it) {
    found_galleries.insert(it->second.AbsolutePath());
    DCHECK(!ContainsKey(bad_galleries, it->second.AbsolutePath()));
  }
  for (std::set<base::FilePath>::const_iterator it = expected_galleries.begin();
       it != expected_galleries.end();
       ++it) {
    DCHECK(ContainsKey(found_galleries, *it));
  }
}

TEST_F(MediaScanManagerTest, UpdateExistingScanResults) {
  size_t galleries_before = gallery_count();

  MediaGalleryPrefId ungranted_scan =
      AddGallery("uscan", MediaGalleryPrefInfo::kScanResult, 1, 0, 0);
  MediaGalleryPrefId granted_scan =
      AddGallery("gscan", MediaGalleryPrefInfo::kScanResult, 0, 2, 0);
  gallery_prefs()->SetGalleryPermissionForExtension(*extension(), granted_scan,
                                                    true);
  EXPECT_EQ(galleries_before + 2, gallery_count());

  // Run once with no scan results. "uscan" should go away and "gscan" should
  // have its scan counts updated.
  MediaFolderFinder::MediaFolderFinderResults found_folders;
  SetFindFoldersResults(true, found_folders);

  MediaGalleryScanResult file_counts;
  SetExpectedScanResults(0 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 1, gallery_count());
  CheckFileCounts(granted_scan, 0, 0, 0);

  MediaGalleryPrefId id =
      AddGallery("uscan", MediaGalleryPrefInfo::kScanResult, 1, 1, 1);
  EXPECT_NE(id, ungranted_scan);
  ungranted_scan = id;

  // Add scan results near the existing scan results.
  file_counts.audio_count = 0;
  file_counts.image_count = 0;
  file_counts.video_count = 7;
  base::FilePath path;
  MakeTestFolder("uscan", &path);
  found_folders[path] = file_counts;

  file_counts.video_count = 11;
  MakeTestFolder("gscan/dir1", &path);
  found_folders[path] = file_counts;

  MakeTestFolder("junk", &path);

  SetFindFoldersResults(true, found_folders);
  file_counts.video_count = 7;
  SetExpectedScanResults(1 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 2, gallery_count());
  CheckFileCounts(granted_scan, 0, 0, 11);
  // The new scan result should be one more than it's previous id.
  CheckFileCounts(ungranted_scan + 1, 0, 0, 7);
}

TEST_F(MediaScanManagerTest, UpdateExistingCounts) {
  size_t galleries_before = gallery_count();

  MediaGalleryPrefId auto_id =
      AddGallery("auto", MediaGalleryPrefInfo::kAutoDetected, 1, 0, 0);
  MediaGalleryPrefId user_id =
      AddGallery("user", MediaGalleryPrefInfo::kUserAdded, 0, 2, 0);
  MediaGalleryPrefId scan_id =
      AddGallery("scan", MediaGalleryPrefInfo::kScanResult, 0, 0, 3);
  // Grant permission so this one isn't removed and readded.
  gallery_prefs()->SetGalleryPermissionForExtension(*extension(), scan_id,
                                                    true);
  CheckFileCounts(auto_id, 1, 0, 0);
  CheckFileCounts(user_id, 0, 2, 0);
  CheckFileCounts(scan_id, 0, 0, 3);

  MediaFolderFinder::MediaFolderFinderResults found_folders;
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 4;
  base::FilePath path;
  MakeTestFolder("auto/dir1", &path);
  found_folders[path] = file_counts;

  file_counts.audio_count = 6;
  MakeTestFolder("scan", &path);
  found_folders[path] = file_counts;

  MakeTestFolder("junk", &path);

  file_counts.audio_count = 5;
  MakeTestFolder("user/dir2", &path);
  found_folders[path] = file_counts;

  SetFindFoldersResults(true, found_folders);

  file_counts.audio_count = 0;
  SetExpectedScanResults(0 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 3, gallery_count());
  CheckFileCounts(auto_id, 4, 0, 0);
  CheckFileCounts(user_id, 5, 0, 0);
  CheckFileCounts(scan_id, 6, 0, 0);

  EXPECT_EQ(1U, found_folders.erase(path));
  SetFindFoldersResults(true, found_folders);
  SetExpectedScanResults(0 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 3, gallery_count());
  CheckFileCounts(auto_id, 4, 0, 0);
  CheckFileCounts(user_id, 0, 0, 0);
  CheckFileCounts(scan_id, 6, 0, 0);
}

TEST_F(MediaScanManagerTest, Graylist) {
  size_t galleries_before = gallery_count();
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 1;
  file_counts.image_count = 2;
  file_counts.video_count = 3;
  base::FilePath path;
  MakeTestFolder("found_media_folder", &path);
  base::ScopedPathOverride scoped_fake_home_dir_override(base::DIR_HOME, path);

  const size_t kGalleriesAdded = 3;
  MediaFolderFinder::MediaFolderFinderResults found_folders;
  MakeTestFolder("found_media_folder/dir1", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("found_media_folder/dir2", &path);
  found_folders[path] = file_counts;
  MakeTestFolder("found_media_folder/dir3", &path);
  found_folders[path] = file_counts;
  SetFindFoldersResults(true, found_folders);

  file_counts.audio_count *= kGalleriesAdded;
  file_counts.image_count *= kGalleriesAdded;
  file_counts.video_count *= kGalleriesAdded;
  SetExpectedScanResults(kGalleriesAdded, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + kGalleriesAdded, gallery_count());
}
