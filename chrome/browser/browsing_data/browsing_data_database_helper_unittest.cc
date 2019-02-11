// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_database_helper.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using storage::DatabaseIdentifier;

class CannedBrowsingDataDatabaseHelperTest : public testing::Test {
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(CannedBrowsingDataDatabaseHelperTest, Empty) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper(
      new CannedBrowsingDataDatabaseHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin));
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedBrowsingDataDatabaseHelperTest, Delete) {
  TestingProfile profile;

  const GURL origin1("http://host1:9000");
  const GURL origin2("http://example.com");
  const GURL origin3("http://foo.example.com");

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper(
      new CannedBrowsingDataDatabaseHelper(&profile));

  EXPECT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin1));
  helper->Add(url::Origin::Create(origin2));
  helper->Add(url::Origin::Create(origin3));
  EXPECT_EQ(3u, helper->GetCount());
  helper->DeleteDatabase(url::Origin::Create(origin2));
  EXPECT_EQ(2u, helper->GetCount());
  helper->DeleteDatabase(url::Origin::Create(origin2));
  EXPECT_EQ(2u, helper->GetCount());
}

TEST_F(CannedBrowsingDataDatabaseHelperTest, IgnoreExtensionsAndDevTools) {
  TestingProfile profile;

  const GURL origin1("chrome-extension://abcdefghijklmnopqrstuvwxyz/");
  const GURL origin2("chrome-devtools://abcdefghijklmnopqrstuvwxyz/");

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper(
      new CannedBrowsingDataDatabaseHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin1));
  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin2));
  ASSERT_TRUE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

}  // namespace
