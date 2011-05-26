// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "chrome/browser/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data_helper_browsertest.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

namespace {

typedef BrowsingDataHelperCallback<BrowsingDataFileSystemHelper::FileSystemInfo>
    TestCompletionCallback;

const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:1/";
const char kTestOrigin3[] = "http://host3:1/";

const char kTestOriginExtension[] =
  "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj:1/";

const int kEmptyFileSystemSize = fileapi::FileSystemUsageCache::kUsageFileSize;

class BrowsingDataFileSystemHelperTest : public InProcessBrowserTest {
 public:
  virtual void PopulateTestData() {
    const GURL origin1(kTestOrigin1);
    const GURL origin2(kTestOrigin2);
    const GURL origin3(kTestOrigin3);

    sandbox_ = testing_profile_.GetFileSystemContext()->
               path_manager()->sandbox_provider();

    CreateDirectoryForOriginAndType(origin1, fileapi::kFileSystemTypeTemporary);
    CreateDirectoryForOriginAndType(origin2,
        fileapi::kFileSystemTypePersistent);
    CreateDirectoryForOriginAndType(origin3, fileapi::kFileSystemTypeTemporary);
    CreateDirectoryForOriginAndType(origin3,
        fileapi::kFileSystemTypePersistent);
  }

 protected:
  void CreateDirectoryForOriginAndType(const GURL& origin,
                                       fileapi::FileSystemType type) {
    FilePath target = sandbox_->ValidateFileSystemRootAndGetPathOnFileThread(
        origin, type, FilePath(), true);
    ASSERT_TRUE(file_util::CreateDirectory(target));
  }

  TestingProfile testing_profile_;
  fileapi::SandboxMountPointProvider* sandbox_;
};

// Called back by BrowsingDataFileSystemHelper on the UI thread once the
// database information has been retrieved.
class StopTestOnCallback {
 public:
  explicit StopTestOnCallback(
      BrowsingDataFileSystemHelper* file_system_helper)
      : file_system_helper_(file_system_helper) {
    DCHECK(file_system_helper_);
  }

  void CallbackFetchData(
      const std::vector<BrowsingDataFileSystemHelper::FileSystemInfo>&
          file_system_info_list) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_EQ(3UL, file_system_info_list.size());

    // Order is arbitrary, verify all three origins.
    bool test_hosts_found[3] = {false, false, false};
    for (size_t i = 0; i < file_system_info_list.size(); i++) {
      BrowsingDataFileSystemHelper::FileSystemInfo info =
          file_system_info_list.at(i);
      if (info.origin == GURL(kTestOrigin1)) {
        test_hosts_found[0] = true;
        EXPECT_FALSE(info.has_persistent);
        EXPECT_TRUE(info.has_temporary);
        EXPECT_EQ(0, info.usage_persistent);
        EXPECT_EQ(kEmptyFileSystemSize, info.usage_temporary);
      } else if (info.origin == GURL(kTestOrigin2)) {
        test_hosts_found[1] = true;
        EXPECT_TRUE(info.has_persistent);
        EXPECT_FALSE(info.has_temporary);
        EXPECT_EQ(kEmptyFileSystemSize, info.usage_persistent);
        EXPECT_EQ(0, info.usage_temporary);
      } else if (info.origin == GURL(kTestOrigin3)) {
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
    MessageLoop::current()->Quit();
  }

  void CallbackDeleteData(
      const std::vector<BrowsingDataFileSystemHelper::FileSystemInfo>&
          file_system_info_list) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_EQ(1UL, file_system_info_list.size());
    BrowsingDataFileSystemHelper::FileSystemInfo info =
        file_system_info_list.at(0);
    EXPECT_EQ(info.origin, GURL(kTestOrigin3));
    EXPECT_TRUE(info.has_persistent);
    EXPECT_TRUE(info.has_temporary);
    EXPECT_EQ(kEmptyFileSystemSize, info.usage_persistent);
    EXPECT_EQ(kEmptyFileSystemSize, info.usage_temporary);
    MessageLoop::current()->Quit();
  }

 private:
  BrowsingDataFileSystemHelper* file_system_helper_;
};


IN_PROC_BROWSER_TEST_F(BrowsingDataFileSystemHelperTest, FetchData) {
  PopulateTestData();
  scoped_refptr<BrowsingDataFileSystemHelper> file_system_helper(
      BrowsingDataFileSystemHelper::Create(&testing_profile_));
  StopTestOnCallback stop_test_on_callback(file_system_helper);
  file_system_helper->StartFetching(
      NewCallback(&stop_test_on_callback,
      &StopTestOnCallback::CallbackFetchData));
  // Blocks until StopTestOnCallback::CallbackFetchData is notified.
  ui_test_utils::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(BrowsingDataFileSystemHelperTest, DeleteData) {
  PopulateTestData();
  scoped_refptr<BrowsingDataFileSystemHelper> file_system_helper(
      BrowsingDataFileSystemHelper::Create(&testing_profile_));
  StopTestOnCallback stop_test_on_callback(file_system_helper);
  file_system_helper->DeleteFileSystemOrigin(GURL(kTestOrigin1));
  file_system_helper->DeleteFileSystemOrigin(GURL(kTestOrigin2));
  file_system_helper->StartFetching(
      NewCallback(&stop_test_on_callback,
      &StopTestOnCallback::CallbackDeleteData));
  // Blocks until StopTestOnCallback::CallbackDeleteData is notified.
  ui_test_utils::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(BrowsingDataFileSystemHelperTest, CannedAddFileSystem) {
  const GURL origin1(kTestOrigin1);
  const GURL origin2(kTestOrigin2);

  scoped_refptr<CannedBrowsingDataFileSystemHelper> helper(
      new CannedBrowsingDataFileSystemHelper(&testing_profile_));
  helper->AddFileSystem(origin1, fileapi::kFileSystemTypePersistent, 200);
  helper->AddFileSystem(origin2, fileapi::kFileSystemTypeTemporary, 100);

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));

  std::vector<BrowsingDataFileSystemHelper::FileSystemInfo> result =
      callback.result();

  EXPECT_EQ(2U, result.size());
  EXPECT_EQ(origin1, result[0].origin);
  EXPECT_TRUE(result[0].has_persistent);
  EXPECT_FALSE(result[0].has_temporary);
  EXPECT_EQ(200, result[0].usage_persistent);
  EXPECT_EQ(0, result[0].usage_temporary);
  EXPECT_EQ(origin2, result[1].origin);
  EXPECT_FALSE(result[1].has_persistent);
  EXPECT_TRUE(result[1].has_temporary);
  EXPECT_EQ(0, result[1].usage_persistent);
  EXPECT_EQ(100, result[1].usage_temporary);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataFileSystemHelperTest, CannedUnique) {
  const GURL origin3(kTestOrigin3);

  scoped_refptr<CannedBrowsingDataFileSystemHelper> helper(
      new CannedBrowsingDataFileSystemHelper(&testing_profile_));
  helper->AddFileSystem(origin3, fileapi::kFileSystemTypePersistent, 200);
  helper->AddFileSystem(origin3, fileapi::kFileSystemTypeTemporary, 100);

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));

  std::vector<BrowsingDataFileSystemHelper::FileSystemInfo> result =
      callback.result();

  EXPECT_EQ(1U, result.size());
  EXPECT_EQ(origin3, result[0].origin);
  EXPECT_TRUE(result[0].has_persistent);
  EXPECT_TRUE(result[0].has_temporary);
  EXPECT_EQ(200, result[0].usage_persistent);
  EXPECT_EQ(100, result[0].usage_temporary);
}

}  // namespace
