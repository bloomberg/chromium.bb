// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_folder_finder.h"

#include <set>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_galleries/media_scan_types.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"

class MediaFolderFinderTest : public testing::Test {
 public:
  MediaFolderFinderTest() {
  }

  virtual ~MediaFolderFinderTest() {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(fake_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_EQ(NULL, media_folder_finder_.get());
  }

 protected:
  void CreateMediaFolderFinder(
      const std::vector<base::FilePath> roots,
      bool expected_success,
      const MediaFolderFinder::MediaFolderFinderResults& expected_results) {
    EXPECT_EQ(NULL, media_folder_finder_.get());
    received_results_ = false;
    expected_success_ = expected_success;
    expected_results_ = expected_results;
    media_folder_finder_.reset(
        new MediaFolderFinder(base::Bind(&MediaFolderFinderTest::OnGotResults,
                                         base::Unretained(this))));
    media_folder_finder_->SetRootsForTesting(roots);
  }

  void StartScan() {
    media_folder_finder_->StartScan();
  }

  void DeleteMediaFolderFinder() {
    EXPECT_TRUE(media_folder_finder_.get() != NULL);
    media_folder_finder_.reset();
  }

  bool received_results() const {
    return received_results_;
  }

  const base::FilePath& fake_dir() const {
    return fake_dir_.path();
  }

  void CreateTestDir(const base::FilePath& parent_dir) {
    if (parent_dir == fake_dir())
      return;

    ASSERT_TRUE(fake_dir().IsParent(parent_dir));
    ASSERT_TRUE(base::CreateDirectory(parent_dir));
  }

  void CreateTestFile(const base::FilePath& parent_dir,
                      MediaGalleryScanFileType type,
                      size_t count,
                      bool big,
                      MediaFolderFinder::MediaFolderFinderResults* results) {
    CreateTestDir(parent_dir);

    std::string extension;
    size_t filesize;
    MediaGalleryScanResult& result = (*results)[parent_dir];
    switch (type) {
      case MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE:
        extension = "jpg";
        filesize = 10;
        result.image_count += count;
        break;
      case MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO:
        extension = "wav";
        filesize = 20;
        result.audio_count += count;
        break;
      case MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO:
        extension = "avi";
        filesize = 30;
        result.video_count += count;
        break;
      case MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN:
        extension = "txt";
        filesize = 10;
        if (IsEmptyScanResult(result))
          results->erase(parent_dir);
        break;
      default:
        NOTREACHED();
        return;
    }
    if (big)
      filesize *= 100000;

    for (size_t i = 0; i < count; ++i) {
      base::FilePath test_file(parent_dir.AppendASCII("dummy." + extension));
      int uniquifier =
          base::GetUniquePathNumber(test_file, base::FilePath::StringType());
      if (uniquifier > 0) {
        test_file = test_file.InsertBeforeExtensionASCII(
            base::StringPrintf(" (%d)", uniquifier));
        filesize += uniquifier;
      }

      std::string dummy_data;
      dummy_data.resize(filesize);

      int bytes_written =
          base::WriteFile(test_file, dummy_data.c_str(), filesize);
      ASSERT_GE(bytes_written, 0);
      ASSERT_EQ(filesize, static_cast<size_t>(bytes_written));
    }
  }

  void RunLoopUntilReceivedCallback() {
    while (!received_results())
      content::RunAllBlockingPoolTasksUntilIdle();
  }

 private:
  void OnGotResults(
      bool success,
      const MediaFolderFinder::MediaFolderFinderResults& results) {
    received_results_ = true;
    EXPECT_EQ(expected_success_, success);
    std::set<base::FilePath> expected_keys =
        GetKeysFromResults(expected_results_);
    ASSERT_EQ(expected_keys, GetKeysFromResults(results));
    for (MediaFolderFinder::MediaFolderFinderResults::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      const base::FilePath& folder = it->first;
      const MediaGalleryScanResult& expected = it->second;
      const MediaGalleryScanResult& actual = results.find(folder)->second;
      EXPECT_EQ(expected.image_count, actual.image_count)
          << " Image count for " << folder.value();
      EXPECT_EQ(expected.audio_count, actual.audio_count)
          << " Audio count for " << folder.value();
      EXPECT_EQ(expected.video_count, actual.video_count)
          << " Video count for " << folder.value();
    }
  }

  std::set<base::FilePath> GetKeysFromResults(
      const MediaFolderFinder::MediaFolderFinderResults& results) {
    std::set<base::FilePath> keys;
    for (MediaFolderFinder::MediaFolderFinderResults::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      keys.insert(it->first);
    }
    return keys;
  }

  content::TestBrowserThreadBundle thread_bundle_;

  base::ScopedTempDir fake_dir_;

  scoped_ptr<MediaFolderFinder> media_folder_finder_;

  bool expected_success_;
  MediaFolderFinder::MediaFolderFinderResults expected_results_;
  bool received_results_;

  DISALLOW_COPY_AND_ASSIGN(MediaFolderFinderTest);
};

TEST_F(MediaFolderFinderTest, NoScan) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  folders.push_back(fake_dir());
  CreateMediaFolderFinder(folders, false, expected_results);
  DeleteMediaFolderFinder();
  EXPECT_TRUE(received_results());
}

TEST_F(MediaFolderFinderTest, ScanAndCancel) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  folders.push_back(fake_dir());
  CreateMediaFolderFinder(folders, false, expected_results);
  StartScan();
  DeleteMediaFolderFinder();
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_TRUE(received_results());
}

TEST_F(MediaFolderFinderTest, ScanNothing) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  CreateMediaFolderFinder(folders, true, expected_results);
  StartScan();
  RunLoopUntilReceivedCallback();
  DeleteMediaFolderFinder();
}

TEST_F(MediaFolderFinderTest, EmptyScan) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  folders.push_back(fake_dir());
  CreateMediaFolderFinder(folders, true, expected_results);
  StartScan();
  RunLoopUntilReceivedCallback();
  DeleteMediaFolderFinder();
}

TEST_F(MediaFolderFinderTest, ScanMediaFiles) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  folders.push_back(fake_dir());

  base::FilePath dir1 = fake_dir().AppendASCII("dir1");
  base::FilePath dir2 = fake_dir().AppendASCII("dir2");
  base::FilePath dir2_3 = dir2.AppendASCII("dir2_3");
  base::FilePath dir2_4 = dir2.AppendASCII("dir2_4");
  base::FilePath dir2_4_5 = dir2_4.AppendASCII("dir2_4_5");
  base::FilePath dir2_4_empty = dir2_4.AppendASCII("dir2_4_empty");
  base::FilePath dir_empty = fake_dir().AppendASCII("dir_empty");

  CreateTestFile(dir1, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 2, true,
                 &expected_results);
  CreateTestFile(dir1, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 1, false,
                 &expected_results);
  CreateTestFile(dir1, MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN, 1, false,
                 &expected_results);
  CreateTestFile(dir2_3, MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO, 4, true,
                 &expected_results);
  CreateTestFile(dir2_3, MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO, 3, false,
                 &expected_results);
  CreateTestFile(dir2_4, MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN, 5, false,
                 &expected_results);
  CreateTestFile(dir2_4_5, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 2, true,
                 &expected_results);
  CreateTestFile(dir2_4_5, MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO, 4, true,
                 &expected_results);
  CreateTestFile(dir2_4_5, MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO, 1, true,
                 &expected_results);
  CreateTestFile(dir2_4_5, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 5, false,
                 &expected_results);
  CreateTestFile(dir2_4_5, MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO, 3, false,
                 &expected_results);
  CreateTestFile(dir2_4_5, MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN, 3, true,
                 &expected_results);
  CreateTestDir(dir2_4_empty);
  CreateTestDir(dir_empty);

  CreateMediaFolderFinder(folders, true, expected_results);
  StartScan();
  RunLoopUntilReceivedCallback();
  DeleteMediaFolderFinder();
}

TEST_F(MediaFolderFinderTest, SkipHiddenFiles) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  folders.push_back(fake_dir());

  base::FilePath hidden_dir = fake_dir().AppendASCII(".hidden");

  CreateTestFile(hidden_dir, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 2, true,
                 &expected_results);
  expected_results.erase(hidden_dir);

  CreateMediaFolderFinder(folders, true, expected_results);
  StartScan();
  RunLoopUntilReceivedCallback();
  DeleteMediaFolderFinder();
}

TEST_F(MediaFolderFinderTest, ScanIgnoresSmallMediaFiles) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  folders.push_back(fake_dir());

  base::FilePath dir1 = fake_dir().AppendASCII("dir1");
  base::FilePath dir2 = fake_dir().AppendASCII("dir2");
  base::FilePath dir_empty = fake_dir().AppendASCII("dir_empty");

  CreateTestFile(dir1, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 2, true,
                 &expected_results);
  CreateTestFile(dir1, MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO, 1, false,
                 &expected_results);
  CreateTestFile(dir1, MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN, 1, false,
                 &expected_results);
  CreateTestFile(dir2, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 1, false,
                 &expected_results);
  CreateTestFile(dir2, MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO, 3, false,
                 &expected_results);
  CreateTestFile(dir2, MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO, 5, false,
                 &expected_results);
  CreateTestFile(dir2, MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN, 1, true,
                 &expected_results);
  CreateTestDir(dir_empty);
  ASSERT_EQ(1U, expected_results.erase(dir2));

  CreateMediaFolderFinder(folders, true, expected_results);
  StartScan();
  RunLoopUntilReceivedCallback();
  DeleteMediaFolderFinder();
}

TEST_F(MediaFolderFinderTest, Overlap) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  folders.push_back(fake_dir());
  folders.push_back(fake_dir());

  base::FilePath dir1 = fake_dir().AppendASCII("dir1");
  folders.push_back(dir1);
  folders.push_back(dir1);

  CreateTestFile(dir1, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 1, true,
                 &expected_results);

  CreateMediaFolderFinder(folders, true, expected_results);
  StartScan();
  RunLoopUntilReceivedCallback();
  DeleteMediaFolderFinder();
}

TEST_F(MediaFolderFinderTest, Prune) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  folders.push_back(fake_dir());

#if defined(OS_WIN)
  int pruned_dir_key = base::DIR_IE_INTERNET_CACHE;
#elif defined(OS_MACOSX)
  int pruned_dir_key = chrome::DIR_USER_LIBRARY;
#else
  int pruned_dir_key = base::DIR_CACHE;
#endif

  base::FilePath fake_pruned_dir = fake_dir().AppendASCII("dir1");
  base::ScopedPathOverride scoped_fake_pruned_dir_override(pruned_dir_key,
                                                           fake_pruned_dir);

  CreateTestFile(fake_dir(), MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 1, true,
                 &expected_results);
  CreateTestFile(fake_pruned_dir, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 1, true,
                 &expected_results);

  base::FilePath test_dir = fake_pruned_dir.AppendASCII("dir2");
  CreateTestFile(test_dir, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 1, true,
                 &expected_results);

  // |fake_pruned_dir| and |test_dir| are pruned.
  expected_results.erase(fake_pruned_dir);
  expected_results.erase(test_dir);

  CreateMediaFolderFinder(folders, true, expected_results);
  StartScan();
  RunLoopUntilReceivedCallback();
  DeleteMediaFolderFinder();
}

TEST_F(MediaFolderFinderTest, Graylist) {
  MediaFolderFinder::MediaFolderFinderResults expected_results;
  std::vector<base::FilePath> folders;
  folders.push_back(fake_dir());

  base::FilePath fake_home_dir = fake_dir().AppendASCII("dir1");
  base::FilePath test_dir = fake_home_dir.AppendASCII("dir2");
  base::ScopedPathOverride scoped_fake_home_dir_override(base::DIR_HOME,
                                                         fake_home_dir);

  CreateTestFile(fake_dir(), MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 1, true,
                 &expected_results);
  CreateTestFile(fake_home_dir, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 1, true,
                 &expected_results);
  CreateTestFile(test_dir, MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE, 1, true,
                 &expected_results);

  // |fake_home_dir| and its ancestors do not show up in results.
  expected_results.erase(fake_dir());
  expected_results.erase(fake_home_dir);

  CreateMediaFolderFinder(folders, true, expected_results);
  StartScan();
  RunLoopUntilReceivedCallback();
  DeleteMediaFolderFinder();
}
