// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_database_helper.h"

#include "base/file_util.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef testing::Test CannedBrowsingDataDatabaseHelperTest;

TEST_F(CannedBrowsingDataDatabaseHelperTest, Empty) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");
  const char db[] = "db1";

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper(
      new CannedBrowsingDataDatabaseHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->AddDatabase(origin, db, "");
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedBrowsingDataDatabaseHelperTest, IgnoreExtensionsAndDevTools) {
  TestingProfile profile;

  const GURL origin1("chrome-extension://abcdefghijklmnopqrstuvwxyz/");
  const GURL origin2("chrome-devtools://abcdefghijklmnopqrstuvwxyz/");
  const char db[] = "db1";

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper(
      new CannedBrowsingDataDatabaseHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->AddDatabase(origin1, db, "");
  ASSERT_TRUE(helper->empty());
  helper->AddDatabase(origin2, db, "");
  ASSERT_TRUE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

}  // namespace
