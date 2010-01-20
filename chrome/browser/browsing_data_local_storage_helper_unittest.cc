// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/in_process_webkit/webkit_thread.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

static const FilePath::CharType kTestFile0[] =
    FILE_PATH_LITERAL("http_www.chromium.org_0.localstorage");

static const FilePath::CharType kTestFile1[] =
    FILE_PATH_LITERAL("http_www.google.com_0.localstorage");

static const FilePath::CharType kTestFileInvalid[] =
    FILE_PATH_LITERAL("http_www.google.com_localstorage_0.foo");

class BrowsingDataLocalStorageHelperTest : public InProcessBrowserTest {
 protected:
  void CreateLocalStorageFilesForTest() {
    FilePath storage_path = GetLocalStoragePathForTestingProfile();
    file_util::CreateDirectory(storage_path);
    const FilePath::CharType* kFilesToCreate[] = {
        kTestFile0, kTestFile1, kTestFileInvalid,
    };
    for (size_t i = 0; i < arraysize(kFilesToCreate); ++i) {
      FilePath file_path = storage_path.Append(kFilesToCreate[i]);
      file_util::WriteFile(file_path, NULL, 0);
    }
  }

  FilePath GetLocalStoragePathForTestingProfile() {
    FilePath storage_path(testing_profile_.GetPath());
    storage_path = storage_path.Append(
        DOMStorageContext::kLocalStorageDirectory);
    return storage_path;
  }
  TestingProfile testing_profile_;
};

// This class is notified by BrowsingDataLocalStorageHelper on the UI thread
// once it finishes fetching the local storage data.
class StopTestOnCallback {
 public:
  explicit StopTestOnCallback(
      BrowsingDataLocalStorageHelper* local_storage_helper)
      : local_storage_helper_(local_storage_helper) {
    DCHECK(local_storage_helper_);
  }

  void Callback(
      const std::vector<BrowsingDataLocalStorageHelper::LocalStorageInfo>&
      local_storage_info) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    ASSERT_EQ(size_t(2), local_storage_info.size());
    BrowsingDataLocalStorageHelper::LocalStorageInfo info =
        local_storage_info.at(0);
    ASSERT_EQ("http", info.protocol);
    ASSERT_EQ("www.chromium.org", info.host);
    info = local_storage_info.at(1);
    ASSERT_EQ("http", info.protocol);
    ASSERT_EQ("www.google.com", info.host);
    MessageLoop::current()->Quit();
  }

 private:
  BrowsingDataLocalStorageHelper* local_storage_helper_;
};

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest, CallbackCompletes) {
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper(
      new BrowsingDataLocalStorageHelper(&testing_profile_));
  CreateLocalStorageFilesForTest();
  StopTestOnCallback stop_test_on_callback(local_storage_helper);
  local_storage_helper->StartFetching(
      NewCallback(&stop_test_on_callback, &StopTestOnCallback::Callback));
  // Blocks until StopTestOnCallback::Callback is notified.
  ui_test_utils::RunMessageLoop();
}

class WaitForWebKitThread
    : public base::RefCountedThreadSafe<WaitForWebKitThread> {
 public:
  void QuitUiMessageLoopAfterWebKitThreadNotified() {
    ChromeThread::PostTask(ChromeThread::WEBKIT,
                           FROM_HERE,
                           NewRunnableMethod(
                               this, &WaitForWebKitThread::RunInWebKitThread));
  }

 private:
  void RunInWebKitThread() {
    ChromeThread::PostTask(ChromeThread::UI,
                           FROM_HERE,
                           NewRunnableMethod(
                               this, &WaitForWebKitThread::RunInUiThread));
  }

  void RunInUiThread() {
    MessageLoop::current()->Quit();
  }
};

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest, DeleteSingleFile) {
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper(
      new BrowsingDataLocalStorageHelper(&testing_profile_));
  CreateLocalStorageFilesForTest();
  local_storage_helper->DeleteLocalStorageFile(
      GetLocalStoragePathForTestingProfile().Append(FilePath(kTestFile0)));
  scoped_refptr<WaitForWebKitThread> wait_for_webkit_thread(
      new WaitForWebKitThread);
  wait_for_webkit_thread->QuitUiMessageLoopAfterWebKitThreadNotified();
  // Blocks until WaitForWebKitThread is notified.
  ui_test_utils::RunMessageLoop();
  // Ensure the file has been deleted.
  file_util::FileEnumerator file_enumerator(
      GetLocalStoragePathForTestingProfile(),
      false,
      file_util::FileEnumerator::FILES);
  int num_files = 0;
  for (FilePath file_path = file_enumerator.Next();
       !file_path.empty();
       file_path = file_enumerator.Next()) {
    ASSERT_FALSE(FilePath(kTestFile0) == file_path.BaseName());
    ++num_files;
  }
  ASSERT_EQ(2, num_files);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest, DeleteAllFiles) {
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper(
      new BrowsingDataLocalStorageHelper(&testing_profile_));
  CreateLocalStorageFilesForTest();
  local_storage_helper->DeleteAllLocalStorageFiles();
  scoped_refptr<WaitForWebKitThread> wait_for_webkit_thread(
      new WaitForWebKitThread);
  wait_for_webkit_thread->QuitUiMessageLoopAfterWebKitThreadNotified();
  // Blocks until WaitForWebKitThread is notified.
  ui_test_utils::RunMessageLoop();
  // Ensure the alls files but the one without local storage extension have been
  // deleted.
  file_util::FileEnumerator file_enumerator(
      GetLocalStoragePathForTestingProfile(),
      false,
      file_util::FileEnumerator::FILES);
  for (FilePath file_path = file_enumerator.Next();
       !file_path.empty();
       file_path = file_enumerator.Next()) {
    ASSERT_TRUE(FilePath(kTestFileInvalid) == file_path.BaseName());
  }
}
