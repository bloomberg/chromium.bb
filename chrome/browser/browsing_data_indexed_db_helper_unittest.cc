// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/test/testing_profile.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

namespace {

class TestCompletionCallback {
 public:
  TestCompletionCallback() : has_result_(false) {}

  bool has_result() const { return has_result_; }

  const std::vector<BrowsingDataIndexedDBHelper::IndexedDBInfo>& result() {
    return result_;
  }

  void callback(
      const std::vector<BrowsingDataIndexedDBHelper::IndexedDBInfo>& info) {
    has_result_ = true;
    result_ = info;
  }

 private:
  bool has_result_;
  std::vector<BrowsingDataIndexedDBHelper::IndexedDBInfo> result_;

  DISALLOW_COPY_AND_ASSIGN(TestCompletionCallback);
};

TEST(CannedBrowsingDataIndexedDBHelperTest, AddIndexedDB) {
  TestingProfile profile;

  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");
  const string16 name(ASCIIToUTF16("name"));
  const string16 description(ASCIIToUTF16("description"));
  const FilePath::CharType file1[] =
      FILE_PATH_LITERAL("http_host1_1@name.indexeddb");
  const FilePath::CharType file2[] =
      FILE_PATH_LITERAL("http_host2_1@name.indexeddb");

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(&profile));
  helper->AddIndexedDB(origin1, name, description);
  helper->AddIndexedDB(origin2, name, description);

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));
  ASSERT_TRUE(callback.has_result());

  std::vector<BrowsingDataIndexedDBHelper::IndexedDBInfo> result =
      callback.result();

  ASSERT_EQ(2U, result.size());
  EXPECT_EQ(FilePath(file1).value(), result[0].file_path.BaseName().value());
  EXPECT_EQ(FilePath(file2).value(), result[1].file_path.BaseName().value());
}

TEST(CannedBrowsingDataIndexedDBHelperTest, Unique) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");
  const string16 name(ASCIIToUTF16("name"));
  const string16 description(ASCIIToUTF16("description"));
  const FilePath::CharType file[] =
      FILE_PATH_LITERAL("http_host1_1@name.indexeddb");

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(&profile));
  helper->AddIndexedDB(origin, name, description);
  helper->AddIndexedDB(origin, name, description);

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));
  ASSERT_TRUE(callback.has_result());

  std::vector<BrowsingDataIndexedDBHelper::IndexedDBInfo> result =
      callback.result();

  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(FilePath(file).value(), result[0].file_path.BaseName().value());
}

TEST(CannedBrowsingDataIndexedDBHelperTest, Empty) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");
  const string16 name(ASCIIToUTF16("name"));
  const string16 description(ASCIIToUTF16("description"));

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->AddIndexedDB(origin, name, description);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

} // namespace
