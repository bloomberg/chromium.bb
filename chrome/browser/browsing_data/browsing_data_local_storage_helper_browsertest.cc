// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/test/thread_test_helper.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_helper_browsertest.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/dom_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;

namespace {
typedef
    BrowsingDataHelperCallback<BrowsingDataLocalStorageHelper::LocalStorageInfo>
        TestCompletionCallback;

const base::FilePath::CharType kTestFile0[] =
    FILE_PATH_LITERAL("http_www.chromium.org_0.localstorage");

const char kOriginOfTestFile0[] = "http://www.chromium.org/";

const base::FilePath::CharType kTestFile1[] =
    FILE_PATH_LITERAL("http_www.google.com_0.localstorage");

const base::FilePath::CharType kTestFileInvalid[] =
    FILE_PATH_LITERAL("http_www.google.com_localstorage_0.foo");

// This is only here to test that extension state is not listed by the helper.
const base::FilePath::CharType kTestFileExtension[] = FILE_PATH_LITERAL(
    "chrome-extension_behllobkkfkfnphdnhnkndlbkcpglgmj_0.localstorage");

class BrowsingDataLocalStorageHelperTest : public InProcessBrowserTest {
 protected:
  void CreateLocalStorageFilesForTest() {
    // Note: This helper depends on details of how the dom_storage library
    // stores data in the host file system.
    base::FilePath storage_path = GetLocalStoragePathForTestingProfile();
    file_util::CreateDirectory(storage_path);
    const base::FilePath::CharType* kFilesToCreate[] = {
        kTestFile0, kTestFile1, kTestFileInvalid, kTestFileExtension
    };
    for (size_t i = 0; i < arraysize(kFilesToCreate); ++i) {
      base::FilePath file_path = storage_path.Append(kFilesToCreate[i]);
      file_util::WriteFile(file_path, NULL, 0);
    }
  }

  base::FilePath GetLocalStoragePathForTestingProfile() {
    return browser()->profile()->GetPath().AppendASCII("Local Storage");
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
        ASSERT_TRUE(info->origin_url.SchemeIs("http"));
        if (info->origin_url.host() == kTestHosts[i]) {
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
  content::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest, DeleteSingleFile) {
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper(
      new BrowsingDataLocalStorageHelper(browser()->profile()));
  CreateLocalStorageFilesForTest();
  local_storage_helper->DeleteOrigin(GURL(kOriginOfTestFile0));
  BrowserThread::GetBlockingPool()->FlushForTesting();

  // Ensure the file has been deleted.
  file_util::FileEnumerator file_enumerator(
      GetLocalStoragePathForTestingProfile(),
      false,
      file_util::FileEnumerator::FILES);
  int num_files = 0;
  for (base::FilePath file_path = file_enumerator.Next();
       !file_path.empty();
       file_path = file_enumerator.Next()) {
    ASSERT_FALSE(base::FilePath(kTestFile0) == file_path.BaseName());
    ++num_files;
  }
  ASSERT_EQ(3, num_files);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest,
                       CannedAddLocalStorage) {
  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");

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
  EXPECT_EQ(origin1, info->origin_url);
  info++;
  EXPECT_EQ(origin2, info->origin_url);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataLocalStorageHelperTest, CannedUnique) {
  const GURL origin("http://host1:1/");

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
  EXPECT_EQ(origin, result.begin()->origin_url);
}
}  // namespace
