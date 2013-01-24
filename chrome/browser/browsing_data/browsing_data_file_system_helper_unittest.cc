// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/platform_file.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_thread.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {

// Shorter names for fileapi::* constants.
const fileapi::FileSystemType kTemporary = fileapi::kFileSystemTypeTemporary;
const fileapi::FileSystemType kPersistent = fileapi::kFileSystemTypePersistent;

// We'll use these three distinct origins for testing, both as strings and as
// GURLs in appropriate contexts.
const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:2/";
const char kTestOrigin3[] = "http://host3:3/";

// Extensions and Devtools should be ignored.
const char kTestOriginExt[] = "chrome-extension://abcdefghijklmnopqrstuvwxyz/";
const char kTestOriginDevTools[] = "chrome-devtools://abcdefghijklmnopqrstuvw/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);
const GURL kOriginExt(kTestOriginExt);
const GURL kOriginDevTools(kTestOriginDevTools);

// TODO(mkwst): Update this size once the discussion in http://crbug.com/86114
// is concluded.
const int kEmptyFileSystemSize = 0;

typedef std::list<BrowsingDataFileSystemHelper::FileSystemInfo>
    FileSystemInfoList;
typedef scoped_ptr<FileSystemInfoList> ScopedFileSystemInfoList;

// The FileSystem APIs are all asynchronous; this testing class wraps up the
// boilerplate code necessary to deal with waiting for responses. In a nutshell,
// any async call whose response we want to test ought to be followed by a call
// to BlockUntilNotified(), which will (shockingly!) block until Notify() is
// called. For this to work, you'll need to ensure that each async call is
// implemented as a class method that that calls Notify() at an appropriate
// point.
class BrowsingDataFileSystemHelperTest : public testing::Test {
 public:
  BrowsingDataFileSystemHelperTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB, &message_loop_),
        webkit_thread_(BrowserThread::WEBKIT_DEPRECATED, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        file_user_blocking_thread_(
            BrowserThread::FILE_USER_BLOCKING, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
    profile_.reset(new TestingProfile());

    helper_ = BrowsingDataFileSystemHelper::Create(
        BrowserContext::GetDefaultStoragePartition(profile_.get())->
            GetFileSystemContext());
    message_loop_.RunUntilIdle();
    canned_helper_ = new CannedBrowsingDataFileSystemHelper(profile_.get());
  }
  virtual ~BrowsingDataFileSystemHelperTest() {
    // Avoid memory leaks.
    profile_.reset();
    message_loop_.RunUntilIdle();
  }

  TestingProfile* GetProfile() {
    return profile_.get();
  }

  // Blocks on the current MessageLoop until Notify() is called.
  void BlockUntilNotified() {
    MessageLoop::current()->Run();
  }

  // Unblocks the current MessageLoop. Should be called in response to some sort
  // of async activity in a callback method.
  void Notify() {
    MessageLoop::current()->Quit();
  }

  // Callback that should be executed in response to
  // fileapi::SandboxMountPointProvider::ValidateFileSystemRoot
  void ValidateFileSystemCallback(base::PlatformFileError error) {
    validate_file_system_result_ = error;
    Notify();
  }

  // Calls fileapi::SandboxMountPointProvider::ValidateFileSystemRootAndGetURL
  // to verify the existence of a file system for a specified type and origin,
  // blocks until a response is available, then returns the result
  // synchronously to it's caller.
  bool FileSystemContainsOriginAndType(const GURL& origin,
                                       fileapi::FileSystemType type) {
    sandbox_->ValidateFileSystemRoot(
        origin, type, false,
        base::Bind(
            &BrowsingDataFileSystemHelperTest::ValidateFileSystemCallback,
            base::Unretained(this)));
    BlockUntilNotified();
    return validate_file_system_result_ == base::PLATFORM_FILE_OK;
  }

  // Callback that should be executed in response to StartFetching(), and stores
  // found file systems locally so that they are available via GetFileSystems().
  void CallbackStartFetching(
      const std::list<BrowsingDataFileSystemHelper::FileSystemInfo>&
          file_system_info_list) {
    file_system_info_list_.reset(
        new std::list<BrowsingDataFileSystemHelper::FileSystemInfo>(
            file_system_info_list));
    Notify();
  }

  // Calls StartFetching() on the test's BrowsingDataFileSystemHelper
  // object, then blocks until the callback is executed.
  void FetchFileSystems() {
    helper_->StartFetching(
        base::Bind(&BrowsingDataFileSystemHelperTest::CallbackStartFetching,
                   base::Unretained(this)));
    BlockUntilNotified();
  }

  // Calls StartFetching() on the test's CannedBrowsingDataFileSystemHelper
  // object, then blocks until the callback is executed.
  void FetchCannedFileSystems() {
    canned_helper_->StartFetching(
        base::Bind(&BrowsingDataFileSystemHelperTest::CallbackStartFetching,
                   base::Unretained(this)));
    BlockUntilNotified();
  }

  // Sets up kOrigin1 with a temporary file system, kOrigin2 with a persistent
  // file system, and kOrigin3 with both.
  virtual void PopulateTestFileSystemData() {
    sandbox_ = BrowserContext::GetDefaultStoragePartition(profile_.get())->
        GetFileSystemContext()->sandbox_provider();

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

  // Uses the fileapi methods to create a filesystem of a given type for a
  // specified origin.
  void CreateDirectoryForOriginAndType(const GURL& origin,
                                       fileapi::FileSystemType type) {
    FilePath target = sandbox_->GetFileSystemRootPathOnFileThread(
        fileapi::FileSystemURL::CreateForTest(origin, type, FilePath()), true);
    EXPECT_TRUE(file_util::DirectoryExists(target));
  }

  // Returns a list of the FileSystemInfo objects gathered in the most recent
  // call to StartFetching().
  FileSystemInfoList* GetFileSystems() {
    return file_system_info_list_.get();
  }


  // Temporary storage to pass information back from callbacks.
  base::PlatformFileError validate_file_system_result_;
  ScopedFileSystemInfoList file_system_info_list_;

  scoped_refptr<BrowsingDataFileSystemHelper> helper_;
  scoped_refptr<CannedBrowsingDataFileSystemHelper> canned_helper_;

 private:
  // message_loop_, as well as all the threads associated with it must be
  // defined before profile_ to prevent explosions. The threads also must be
  // defined in the order they're listed here. Oh how I love C++.
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread webkit_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread file_user_blocking_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;

  // We don't own this pointer: don't delete it.
  fileapi::SandboxMountPointProvider* sandbox_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataFileSystemHelperTest);
};

// Verifies that the BrowsingDataFileSystemHelper correctly finds the test file
// system data, and that each file system returned contains the expected data.
TEST_F(BrowsingDataFileSystemHelperTest, FetchData) {
  PopulateTestFileSystemData();

  FetchFileSystems();

  EXPECT_EQ(3UL, file_system_info_list_->size());

  // Order is arbitrary, verify all three origins.
  bool test_hosts_found[3] = {false, false, false};
  for (std::list<BrowsingDataFileSystemHelper::FileSystemInfo>::iterator info =
       file_system_info_list_->begin(); info != file_system_info_list_->end();
       ++info) {
    if (info->origin == kOrigin1) {
        EXPECT_FALSE(test_hosts_found[0]);
        test_hosts_found[0] = true;
        EXPECT_FALSE(info->has_persistent);
        EXPECT_TRUE(info->has_temporary);
        EXPECT_EQ(0, info->usage_persistent);
        EXPECT_EQ(kEmptyFileSystemSize, info->usage_temporary);
    } else if (info->origin == kOrigin2) {
        EXPECT_FALSE(test_hosts_found[1]);
        test_hosts_found[1] = true;
        EXPECT_TRUE(info->has_persistent);
        EXPECT_FALSE(info->has_temporary);
        EXPECT_EQ(kEmptyFileSystemSize, info->usage_persistent);
        EXPECT_EQ(0, info->usage_temporary);
    } else if (info->origin == kOrigin3) {
        EXPECT_FALSE(test_hosts_found[2]);
        test_hosts_found[2] = true;
        EXPECT_TRUE(info->has_persistent);
        EXPECT_TRUE(info->has_temporary);
        EXPECT_EQ(kEmptyFileSystemSize, info->usage_persistent);
        EXPECT_EQ(kEmptyFileSystemSize, info->usage_temporary);
    } else {
        ADD_FAILURE() << info->origin.spec() << " isn't an origin we added.";
    }
  }
  for (size_t i = 0; i < arraysize(test_hosts_found); i++) {
    EXPECT_TRUE(test_hosts_found[i]);
  }
}

// Verifies that the BrowsingDataFileSystemHelper correctly deletes file
// systems via DeleteFileSystemOrigin().
TEST_F(BrowsingDataFileSystemHelperTest, DeleteData) {
  PopulateTestFileSystemData();

  helper_->DeleteFileSystemOrigin(kOrigin1);
  helper_->DeleteFileSystemOrigin(kOrigin2);

  FetchFileSystems();

  EXPECT_EQ(1UL, file_system_info_list_->size());
  BrowsingDataFileSystemHelper::FileSystemInfo info =
      *(file_system_info_list_->begin());
  EXPECT_EQ(kOrigin3, info.origin);
  EXPECT_TRUE(info.has_persistent);
  EXPECT_TRUE(info.has_temporary);
  EXPECT_EQ(kEmptyFileSystemSize, info.usage_persistent);
  EXPECT_EQ(kEmptyFileSystemSize, info.usage_temporary);
}

// Verifies that the CannedBrowsingDataFileSystemHelper correctly reports
// whether or not it currently contains file systems.
TEST_F(BrowsingDataFileSystemHelperTest, Empty) {
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->AddFileSystem(kOrigin1, kTemporary, 0);
  ASSERT_FALSE(canned_helper_->empty());
  canned_helper_->Reset();
  ASSERT_TRUE(canned_helper_->empty());
}

// Verifies that AddFileSystem correctly adds file systems, and that both
// the type and usage metadata are reported as provided.
TEST_F(BrowsingDataFileSystemHelperTest, CannedAddFileSystem) {
  canned_helper_->AddFileSystem(kOrigin1, kPersistent, 200);
  canned_helper_->AddFileSystem(kOrigin2, kTemporary, 100);

  FetchCannedFileSystems();

  EXPECT_EQ(2U, file_system_info_list_->size());
  std::list<BrowsingDataFileSystemHelper::FileSystemInfo>::iterator info =
      file_system_info_list_->begin();
  EXPECT_EQ(kOrigin1, info->origin);
  EXPECT_TRUE(info->has_persistent);
  EXPECT_FALSE(info->has_temporary);
  EXPECT_EQ(200, info->usage_persistent);
  EXPECT_EQ(0, info->usage_temporary);

  info++;
  EXPECT_EQ(kOrigin2, info->origin);
  EXPECT_FALSE(info->has_persistent);
  EXPECT_TRUE(info->has_temporary);
  EXPECT_EQ(0, info->usage_persistent);
  EXPECT_EQ(100, info->usage_temporary);
}

// Verifies that the CannedBrowsingDataFileSystemHelper correctly ignores
// extension and devtools schemes.
TEST_F(BrowsingDataFileSystemHelperTest, IgnoreExtensionsAndDevTools) {
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->AddFileSystem(kOriginExt, kTemporary, 0);
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->AddFileSystem(kOriginDevTools, kTemporary, 0);
  ASSERT_TRUE(canned_helper_->empty());
}

}  // namespace
