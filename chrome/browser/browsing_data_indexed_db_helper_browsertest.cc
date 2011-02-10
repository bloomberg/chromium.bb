// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_helper_browsertest.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
typedef BrowsingDataHelperCallback<BrowsingDataIndexedDBHelper::IndexedDBInfo>
    TestCompletionCallback;

class BrowsingDataIndexedDBHelperTest : public InProcessBrowserTest {
 protected:
  TestingProfile testing_profile_;
};

IN_PROC_BROWSER_TEST_F(BrowsingDataIndexedDBHelperTest, CannedAddIndexedDB) {
  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");
  const string16 description(ASCIIToUTF16("description"));
  const FilePath::CharType file1[] =
      FILE_PATH_LITERAL("http_host1_1.indexeddb");
  const FilePath::CharType file2[] =
      FILE_PATH_LITERAL("http_host2_1.indexeddb");

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(&testing_profile_));
  helper->AddIndexedDB(origin1, description);
  helper->AddIndexedDB(origin2, description);

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));

  std::vector<BrowsingDataIndexedDBHelper::IndexedDBInfo> result =
      callback.result();

  ASSERT_EQ(2U, result.size());
  EXPECT_EQ(FilePath(file1).value(), result[0].file_path.BaseName().value());
  EXPECT_EQ(FilePath(file2).value(), result[1].file_path.BaseName().value());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataIndexedDBHelperTest, CannedUnique) {
  const GURL origin("http://host1:1/");
  const string16 description(ASCIIToUTF16("description"));
  const FilePath::CharType file[] =
      FILE_PATH_LITERAL("http_host1_1.indexeddb");

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(&testing_profile_));
  helper->AddIndexedDB(origin, description);
  helper->AddIndexedDB(origin, description);

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));

  std::vector<BrowsingDataIndexedDBHelper::IndexedDBInfo> result =
      callback.result();

  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(FilePath(file).value(), result[0].file_path.BaseName().value());
}
}  // namespace
