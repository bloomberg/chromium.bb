// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/browsing_data_helper_browsertest.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {
typedef
    BrowsingDataHelperCallback<BrowsingDataLocalStorageHelper::LocalStorageInfo>
        TestCompletionCallback;

const FilePath::CharType kTestFile0[] =
    FILE_PATH_LITERAL("http_www.chromium.org_0.localstorage");

const FilePath::CharType kTestFile1[] =
    FILE_PATH_LITERAL("http_www.google.com_0.localstorage");

const FilePath::CharType kTestFileInvalid[] =
    FILE_PATH_LITERAL("http_www.google.com_localstorage_0.foo");

// This is only here to test that extension state is not listed by the helper.
const FilePath::CharType kTestFileExtension[] = FILE_PATH_LITERAL(
    "chrome-extension_behllobkkfkfnphdnhnkndlbkcpglgmj_0.localstorage");

class BrowsingDataLocalStorageHelperTest : public InProcessBrowserTest {
 protected:
  void CreateLocalStorageFilesForTest() {
    FilePath storage_path = GetLocalStoragePathForTestingProfile();
    file_util::CreateDirectory(storage_path);
    const FilePath::CharType* kFilesToCreate[] = {
        kTestFile0, kTestFile1, kTestFileInvalid, kTestFileExtension
    };
    for (size_t i = 0; i < arraysize(kFilesToCreate); ++i) {
      FilePath file_path = storage_path.Append(kFilesToCreate[i]);
      file_util::WriteFile(file_path, NULL, 0);
    }
  }

  FilePath GetLocalStoragePathForTestingProfile() {
    FilePath storage_path(browser()->profile()->GetPath());
    storage_path = storage_path.Append(
        DOMStorageContext::kLocalStorageDirectory);
    return storage_path;
  }
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
      const std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>&
      local_storage_info) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // There's no guarantee on the order, ensure these files are there.
    const char* const kTestHosts[] = {"www.chromium.org", "www.google.com"};
    bool test_hosts_found[arraysize(kTestHosts)] = {false, false};
    ASSERT_EQ(arraysize(kTestHosts), local_storage_info.size());
    typedef std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>
        LocalStorageInfoList;
    for (size_t i = 0; i < arraysize(kTestHosts); ++i) {
      for (LocalStorageInfoList::const_iterator info =
           local_storage_info.begin(); info != local_storage_info.end();
           ++info) {
        ASSERT_EQ("http", info->protocol);
        if (info->host == kTestHosts[i]) {
          ASSERT_FALSE(test_hosts_found[i]);
          test_hosts_found[i] = true;
        }
      }
    }
    for (size_t i = 0; i < arraysize(kTestHosts); ++i) {
      ASSERT_TRUE(test_hosts_found[i]) << kTestHosts[i];
    }
    MessageLoop::current()->Quit();
  }

 private:
  BrowsingDataLocalStorageHelper* local_storage_helper_;
};

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest, CallbackCompletes) {
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper(
      new BrowsingDataLocalStorageHelper(browser()->profile()));
  CreateLocalStorageFilesForTest();
  StopTestOnCallback stop_test_on_callback(local_storage_helper);
  local_storage_helper->StartFetching(
      base::Bind(&StopTestOnCallback::Callback,
                 base::Unretained(&stop_test_on_callback)));
  // Blocks until StopTestOnCallback::Callback is notified.
  ui_test_utils::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest, DeleteSingleFile) {
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper(
      new BrowsingDataLocalStorageHelper(browser()->profile()));
  CreateLocalStorageFilesForTest();
  local_storage_helper->DeleteLocalStorageFile(
      GetLocalStoragePathForTestingProfile().Append(FilePath(kTestFile0)));
  scoped_refptr<base::ThreadTestHelper> wait_for_webkit_thread(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::WEBKIT)));
  ASSERT_TRUE(wait_for_webkit_thread->Run());
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
  ASSERT_EQ(3, num_files);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest,
                       CannedAddLocalStorage) {
  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");
  const FilePath::CharType file1[] =
      FILE_PATH_LITERAL("http_host1_1.localstorage");
  const FilePath::CharType file2[] =
      FILE_PATH_LITERAL("http_host2_1.localstorage");

  scoped_refptr<CannedBrowsingDataLocalStorageHelper> helper(
      new CannedBrowsingDataLocalStorageHelper(browser()->profile()));
  helper->AddLocalStorage(origin1);
  helper->AddLocalStorage(origin2);

  TestCompletionCallback callback;
  helper->StartFetching(
      base::Bind(&TestCompletionCallback::callback,
                 base::Unretained(&callback)));

  std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo> result =
      callback.result();

  ASSERT_EQ(2u, result.size());
  std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator info =
      result.begin();
  EXPECT_EQ(FilePath(file1).value(), info->file_path.BaseName().value());
  info++;
  EXPECT_EQ(FilePath(file2).value(), info->file_path.BaseName().value());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest, CannedUnique) {
  const GURL origin("http://host1:1/");
  const FilePath::CharType file[] =
      FILE_PATH_LITERAL("http_host1_1.localstorage");

  scoped_refptr<CannedBrowsingDataLocalStorageHelper> helper(
      new CannedBrowsingDataLocalStorageHelper(browser()->profile()));
  helper->AddLocalStorage(origin);
  helper->AddLocalStorage(origin);

  TestCompletionCallback callback;
  helper->StartFetching(
      base::Bind(&TestCompletionCallback::callback,
                 base::Unretained(&callback)));

  std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo> result =
      callback.result();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(FilePath(file).value(),
            result.begin()->file_path.BaseName().value());
}
}  // namespace
