// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_database_helper.h"

#include "base/file_util.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestCompletionCallback {
 public:
  TestCompletionCallback()
      : have_result_(false) {
  }

  bool have_result() const { return have_result_; }

  const std::vector<BrowsingDataDatabaseHelper::DatabaseInfo>& result() {
    return result_;
  }

  void callback(const std::vector<
          BrowsingDataDatabaseHelper::DatabaseInfo>& info) {
    have_result_ = true;
    result_ = info;
  }

 private:
  bool have_result_;
  std::vector<BrowsingDataDatabaseHelper::DatabaseInfo> result_;

  DISALLOW_COPY_AND_ASSIGN(TestCompletionCallback);
};
}  // namespace

TEST(CannedBrowsingDataDatabaseTest, AddDatabase) {
  TestingProfile profile;

  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");
  const char origin_str1[] = "http_host1_1";
  const char origin_str2[] = "http_host2_1";
  const char db1[] = "db1";
  const char db2[] = "db2";
  const char db3[] = "db3";

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper =
      new CannedBrowsingDataDatabaseHelper(&profile);
  helper->AddDatabase(origin1, db1, "");
  helper->AddDatabase(origin1, db2, "");
  helper->AddDatabase(origin2, db3, "");

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));
  ASSERT_TRUE(callback.have_result());

  std::vector<BrowsingDataDatabaseHelper::DatabaseInfo> result =
      callback.result();

  ASSERT_EQ(3u, result.size());
  EXPECT_STREQ(origin_str1, result[0].origin_identifier.c_str());
  EXPECT_STREQ(db1, result[0].database_name.c_str());
  EXPECT_STREQ(origin_str1, result[1].origin_identifier.c_str());
  EXPECT_STREQ(db2, result[1].database_name.c_str());
  EXPECT_STREQ(origin_str2, result[2].origin_identifier.c_str());
  EXPECT_STREQ(db3, result[2].database_name.c_str());
}

TEST(CannedBrowsingDataDatabaseTest, Unique) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");
  const char origin_str[] = "http_host1_1";
  const char db[] = "db1";

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper =
      new CannedBrowsingDataDatabaseHelper(&profile);
  helper->AddDatabase(origin, db, "");
  helper->AddDatabase(origin, db, "");

  TestCompletionCallback callback;
  helper->StartFetching(
      NewCallback(&callback, &TestCompletionCallback::callback));
  ASSERT_TRUE(callback.have_result());

  std::vector<BrowsingDataDatabaseHelper::DatabaseInfo> result =
      callback.result();

  ASSERT_EQ(1u, result.size());
  EXPECT_STREQ(origin_str, result[0].origin_identifier.c_str());
  EXPECT_STREQ(db, result[0].database_name.c_str());
}

TEST(CannedBrowsingDataDatabaseTest, Empty) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");
  const char db[] = "db1";

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper =
      new CannedBrowsingDataDatabaseHelper(&profile);

  ASSERT_TRUE(helper->empty());
  helper->AddDatabase(origin, db, "");
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}
