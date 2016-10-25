// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CannedBrowsingDataLocalStorageTest : public testing::Test {
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(CannedBrowsingDataLocalStorageTest, Empty) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");

  scoped_refptr<CannedBrowsingDataLocalStorageHelper> helper(
      new CannedBrowsingDataLocalStorageHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->AddLocalStorage(origin);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedBrowsingDataLocalStorageTest, Delete) {
  TestingProfile profile;

  const GURL origin1("http://host1:9000");
  const GURL origin2("http://example.com");
  const GURL origin3("http://foo.example.com");

  scoped_refptr<CannedBrowsingDataLocalStorageHelper> helper(
      new CannedBrowsingDataLocalStorageHelper(&profile));

  EXPECT_TRUE(helper->empty());
  helper->AddLocalStorage(origin1);
  helper->AddLocalStorage(origin2);
  helper->AddLocalStorage(origin3);
  EXPECT_EQ(3u, helper->GetLocalStorageCount());
  helper->DeleteOrigin(origin2);
  EXPECT_EQ(2u, helper->GetLocalStorageCount());
  helper->DeleteOrigin(origin1);
  EXPECT_EQ(1u, helper->GetLocalStorageCount());

  // Local storage for a suborigin
  // (https://www.chromestatus.com/feature/5569465034997760) should be deleted
  // when the corresponding physical origin is deleted.
  const GURL suborigin_on_origin_3("http-so://suborigin.foo.example.com");
  helper->AddLocalStorage(suborigin_on_origin_3);
  EXPECT_EQ(2u, helper->GetLocalStorageCount());
  helper->DeleteOrigin(origin3);
  EXPECT_EQ(0u, helper->GetLocalStorageCount());
  helper->AddLocalStorage(suborigin_on_origin_3);
  EXPECT_EQ(1u, helper->GetLocalStorageCount());
  helper->DeleteOrigin(origin3);
  EXPECT_EQ(0u, helper->GetLocalStorageCount());

  // Similarly, the suborigin should be deleted when the corresponding
  // physical origin is deleted.
  helper->AddLocalStorage(origin3);
  helper->AddLocalStorage(suborigin_on_origin_3);
  EXPECT_EQ(2u, helper->GetLocalStorageCount());
  helper->DeleteOrigin(suborigin_on_origin_3);
  EXPECT_EQ(0u, helper->GetLocalStorageCount());
}

TEST_F(CannedBrowsingDataLocalStorageTest, IgnoreExtensionsAndDevTools) {
  TestingProfile profile;

  const GURL origin1("chrome-extension://abcdefghijklmnopqrstuvwxyz/");
  const GURL origin2("chrome-devtools://abcdefghijklmnopqrstuvwxyz/");

  scoped_refptr<CannedBrowsingDataLocalStorageHelper> helper(
      new CannedBrowsingDataLocalStorageHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->AddLocalStorage(origin1);
  ASSERT_TRUE(helper->empty());
  helper->AddLocalStorage(origin2);
  ASSERT_TRUE(helper->empty());
}

}  // namespace
