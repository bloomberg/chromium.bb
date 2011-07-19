// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_database_helper.h"

#include "base/file_util.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef TestingBrowserProcessTest CannedBrowsingDataDatabaseTest;

TEST_F(CannedBrowsingDataDatabaseTest, Empty) {
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

}  // namespace
