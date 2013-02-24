// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_helper_browsertest.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
typedef BrowsingDataHelperCallback<BrowsingDataIndexedDBHelper::IndexedDBInfo>
    TestCompletionCallback;

typedef InProcessBrowserTest BrowsingDataIndexedDBHelperTest;

IN_PROC_BROWSER_TEST_F(BrowsingDataIndexedDBHelperTest, CannedAddIndexedDB) {
  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");
  const string16 description(ASCIIToUTF16("description"));

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper());
  helper->AddIndexedDB(origin1, description);
  helper->AddIndexedDB(origin2, description);

  TestCompletionCallback callback;
  helper->StartFetching(
      base::Bind(&TestCompletionCallback::callback,
                 base::Unretained(&callback)));

  std::list<BrowsingDataIndexedDBHelper::IndexedDBInfo> result =
      callback.result();

  ASSERT_EQ(2U, result.size());
  std::list<BrowsingDataIndexedDBHelper::IndexedDBInfo>::iterator info =
      result.begin();
  EXPECT_EQ(origin1, info->origin);
  info++;
  EXPECT_EQ(origin2, info->origin);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataIndexedDBHelperTest, CannedUnique) {
  const GURL origin("http://host1:1/");
  const string16 description(ASCIIToUTF16("description"));

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper());
  helper->AddIndexedDB(origin, description);
  helper->AddIndexedDB(origin, description);

  TestCompletionCallback callback;
  helper->StartFetching(
      base::Bind(&TestCompletionCallback::callback,
                 base::Unretained(&callback)));

  std::list<BrowsingDataIndexedDBHelper::IndexedDBInfo> result =
      callback.result();

  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(origin, result.begin()->origin);
}
}  // namespace
