// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_file_system_helper.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

namespace {

const fileapi::FileSystemType kTemporary = fileapi::kFileSystemTypeTemporary;
const fileapi::FileSystemType kPersistent = fileapi::kFileSystemTypePersistent;

const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:1/";
const char kTestOrigin3[] = "http://host3:1/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);

const int kEmptyFileSystemSize = fileapi::FileSystemUsageCache::kUsageFileSize;

typedef std::vector<BrowsingDataFileSystemHelper::FileSystemInfo>
    FileSystemInfoVector;
typedef scoped_ptr<FileSystemInfoVector> ScopedFileSystemInfoVector;

class BrowsingDataFileSystemHelperTest : public testing::Test {
 public:
  BrowsingDataFileSystemHelperTest()
      : helper_(BrowsingDataFileSystemHelper::Create(&profile_)),
        canned_helper_(new CannedBrowsingDataFileSystemHelper(&profile_)),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }
  virtual ~BrowsingDataFileSystemHelperTest() {}

  TestingProfile* GetProfile() {
    return &profile_;
  }

  void FindFileSystemPathCallback(bool success,
                                  const FilePath& path,
                                  const std::string& name) {
    found_file_system_ = success;
    Notify();
  }

  bool FileSystemContainsOriginAndType(const GURL& origin,
                                       fileapi::FileSystemType type) {
    sandbox_->ValidateFileSystemRootAndGetURL(
        origin, type, false, NewCallback(this,
            &BrowsingDataFileSystemHelperTest::FindFileSystemPathCallback));
    BlockUntilNotified();
    return found_file_system_;
  }

  void CallbackStartFetching(
      const std::vector<BrowsingDataFileSystemHelper::FileSystemInfo>&
          file_system_info_list) {
    file_system_info_list_.reset(
        new std::vector<BrowsingDataFileSystemHelper::FileSystemInfo>(
            file_system_info_list));
    Notify();
  }

  void FetchFileSystems() {
    helper_->StartFetching(NewCallback(this,
        &BrowsingDataFileSystemHelperTest::CallbackStartFetching));
    BlockUntilNotified();
  }

  void FetchCannedFileSystems() {
    canned_helper_->StartFetching(NewCallback(this,
        &BrowsingDataFileSystemHelperTest::CallbackStartFetching));
    BlockUntilNotified();
  }

  virtual void PopulateTestFileSystemData() {
    // Set up kOrigin1 with a temporary file system, kOrigin2 with a persistent
    // file system, and kOrigin3 with both.
    sandbox_ = profile_.GetFileSystemContext()->path_manager()->
        sandbox_provider();

    CreateDirectoryForOriginAndType(kOrigin1, kTemporary);
    CreateDirectoryForOriginAndType(kOrigin2, kPersistent);
    CreateDirectoryForOriginAndType(kOrigin3, kTemporary);
    CreateDirectoryForOriginAndType(kOrigin3, kPersistent);

    EXPECT_FALSE(FileSystemContainsOriginAndType(kOrigin1, kPersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin1, kTemporary));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin2, kPersistent));
    EXPECT_FALSE(FileSystemContainsOriginAndType(kOrigin2, kTemporary));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin3, kPersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin3, kTemporary));
  }

  void CreateDirectoryForOriginAndType(const GURL& origin,
                                       fileapi::FileSystemType type) {
    FilePath target = sandbox_->ValidateFileSystemRootAndGetPathOnFileThread(
        origin, type, FilePath(), true);
    EXPECT_TRUE(file_util::DirectoryExists(target));
  }

  FileSystemInfoVector* GetFileSystems() {
    return file_system_info_list_.get();
  }

  void BlockUntilNotified() {
    MessageLoop::current()->Run();
  }

  void Notify() {
    MessageLoop::current()->Quit();
  }

  // Temporary storage to pass information back from callbacks.
  bool found_file_system_;
  ScopedFileSystemInfoVector file_system_info_list_;

  scoped_refptr<BrowsingDataFileSystemHelper> helper_;
  scoped_refptr<CannedBrowsingDataFileSystemHelper> canned_helper_;

 private:
  // message_loop_, as well as all the threads associated with it must be
  // defined before profile_ to prevent explosions. Oh how I love C++.
  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  BrowserThread io_thread_;
  TestingProfile profile_;

  // We don't own this pointer: don't delete it.
  fileapi::SandboxMountPointProvider* sandbox_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataFileSystemHelperTest);
};

TEST_F(BrowsingDataFileSystemHelperTest, FetchData) {
  PopulateTestFileSystemData();

  FetchFileSystems();

  EXPECT_EQ(3UL, file_system_info_list_->size());

  // Order is arbitrary, verify all three origins.
  bool test_hosts_found[3] = {false, false, false};
  for (size_t i = 0; i < file_system_info_list_->size(); i++) {
    BrowsingDataFileSystemHelper::FileSystemInfo info =
        file_system_info_list_->at(i);
    if (info.origin == kOrigin1) {
        test_hosts_found[0] = true;
        EXPECT_FALSE(info.has_persistent);
        EXPECT_TRUE(info.has_temporary);
        EXPECT_EQ(0, info.usage_persistent);
        EXPECT_EQ(kEmptyFileSystemSize, info.usage_temporary);
    } else if (info.origin == kOrigin2) {
        test_hosts_found[1] = true;
        EXPECT_TRUE(info.has_persistent);
        EXPECT_FALSE(info.has_temporary);
        EXPECT_EQ(kEmptyFileSystemSize, info.usage_persistent);
        EXPECT_EQ(0, info.usage_temporary);
    } else if (info.origin == kOrigin3) {
        test_hosts_found[2] = true;
        EXPECT_TRUE(info.has_persistent);
        EXPECT_TRUE(info.has_temporary);
        EXPECT_EQ(kEmptyFileSystemSize, info.usage_persistent);
        EXPECT_EQ(kEmptyFileSystemSize, info.usage_temporary);
    } else {
        ADD_FAILURE() << info.origin.spec() << " isn't an origin we added.";
    }
  }
  for (size_t i = 0; i < arraysize(test_hosts_found); i++) {
    EXPECT_TRUE(test_hosts_found[i]);
  }
}

TEST_F(BrowsingDataFileSystemHelperTest, DeleteData) {
  PopulateTestFileSystemData();

  helper_->DeleteFileSystemOrigin(kOrigin1);
  helper_->DeleteFileSystemOrigin(kOrigin2);

  FetchFileSystems();

  EXPECT_EQ(3UL, file_system_info_list_->size());
  for (size_t i = 0; i < file_system_info_list_->size(); ++i) {
    BrowsingDataFileSystemHelper::FileSystemInfo info =
        file_system_info_list_->at(0);
    if (info.origin == kOrigin3) {
      EXPECT_TRUE(info.has_persistent);
      EXPECT_TRUE(info.has_temporary);
      EXPECT_EQ(kEmptyFileSystemSize, info.usage_persistent);
      EXPECT_EQ(kEmptyFileSystemSize, info.usage_temporary);
    } else {
      EXPECT_FALSE(info.has_persistent);
      EXPECT_FALSE(info.has_temporary);
    }
  }
}

TEST_F(BrowsingDataFileSystemHelperTest, Empty) {
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->AddFileSystem(kOrigin1, kTemporary, 0);
  ASSERT_FALSE(canned_helper_->empty());
  canned_helper_->Reset();
  ASSERT_TRUE(canned_helper_->empty());
}

TEST_F(BrowsingDataFileSystemHelperTest, CannedAddFileSystem) {
  canned_helper_->AddFileSystem(kOrigin1, kPersistent, 200);
  canned_helper_->AddFileSystem(kOrigin2, kTemporary, 100);

  FetchCannedFileSystems();

  EXPECT_EQ(2U, file_system_info_list_->size());
  EXPECT_EQ(kOrigin1, file_system_info_list_->at(0).origin);
  EXPECT_TRUE(file_system_info_list_->at(0).has_persistent);
  EXPECT_FALSE(file_system_info_list_->at(0).has_temporary);
  EXPECT_EQ(200, file_system_info_list_->at(0).usage_persistent);
  EXPECT_EQ(0, file_system_info_list_->at(0).usage_temporary);
  EXPECT_EQ(kOrigin2, file_system_info_list_->at(1).origin);
  EXPECT_FALSE(file_system_info_list_->at(1).has_persistent);
  EXPECT_TRUE(file_system_info_list_->at(1).has_temporary);
  EXPECT_EQ(0, file_system_info_list_->at(1).usage_persistent);
  EXPECT_EQ(100, file_system_info_list_->at(1).usage_temporary);
}

TEST_F(BrowsingDataFileSystemHelperTest, CannedUnique) {
  canned_helper_->AddFileSystem(kOrigin3, kPersistent, 200);
  canned_helper_->AddFileSystem(kOrigin3, kTemporary, 100);

  FetchCannedFileSystems();

  EXPECT_EQ(1U, file_system_info_list_->size());
  EXPECT_EQ(kOrigin3, file_system_info_list_->at(0).origin);
  EXPECT_TRUE(file_system_info_list_->at(0).has_persistent);
  EXPECT_TRUE(file_system_info_list_->at(0).has_temporary);
  EXPECT_EQ(200, file_system_info_list_->at(0).usage_persistent);
  EXPECT_EQ(100, file_system_info_list_->at(0).usage_temporary);
}

}  // namespace
