// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_local_storage_helper.h"

#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestCompletionCallback {
 public:
  TestCompletionCallback()
      : have_result_(false) {
  }

  bool have_result() const { return have_result_; }

  const std::vector<BrowsingDataLocalStorageHelper::LocalStorageInfo>& result()
  {
    return result_;
  }

  void callback(const std::vector<
          BrowsingDataLocalStorageHelper::LocalStorageInfo>& info) {
    have_result_ = true;
    result_ = info;
  }

 private:
  bool have_result_;
  std::vector<BrowsingDataLocalStorageHelper::LocalStorageInfo> result_;

  DISALLOW_COPY_AND_ASSIGN(TestCompletionCallback);
};
}  // namespace

TEST(CannedBrowsingDataLocalStorageTest, AddLocalStorage) {
  TestingProfile profile;

  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");
  const FilePath::CharType file1[] =
      FILE_PATH_LITERAL("http_host1_1.localstorage");
  const FilePath::CharType file2[] =
      FILE_PATH_LITERAL("http_host2_1.localstorage");

  scoped_refptr<CannedBrowsingDataLocalStorageHelper> helper =
      new CannedBrowsingDataLocalStorageHelper(&profile);
  helper->AddLocalStorage(origin1);
  helper->AddLocalStorage(origin2);

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));
  ASSERT_TRUE(callback.have_result());

  std::vector<BrowsingDataLocalStorageHelper::LocalStorageInfo> result =
      callback.result();

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(FilePath(file1).value(), result[0].file_path.BaseName().value());
  EXPECT_EQ(FilePath(file2).value(), result[1].file_path.BaseName().value());
}

TEST(CannedBrowsingDataLocalStorageTest, Unique) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");
  const FilePath::CharType file[] =
      FILE_PATH_LITERAL("http_host1_1.localstorage");

  scoped_refptr<CannedBrowsingDataLocalStorageHelper> helper =
      new CannedBrowsingDataLocalStorageHelper(&profile);
  helper->AddLocalStorage(origin);
  helper->AddLocalStorage(origin);

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));
  ASSERT_TRUE(callback.have_result());

  std::vector<BrowsingDataLocalStorageHelper::LocalStorageInfo> result =
      callback.result();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(FilePath(file).value(), result[0].file_path.BaseName().value());
}

TEST(CannedBrowsingDataLocalStorageTest, Empty) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");

  scoped_refptr<CannedBrowsingDataLocalStorageHelper> helper =
      new CannedBrowsingDataLocalStorageHelper(&profile);

  ASSERT_TRUE(helper->empty());
  helper->AddLocalStorage(origin);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}
