// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/local_storage_helper.h"

#include "base/bind_helpers.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browsing_data {

namespace {

class CannedLocalStorageTest : public testing::Test {
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CannedLocalStorageTest, Empty) {
  content::TestBrowserContext context;

  const GURL origin("http://host1:1/");

  scoped_refptr<CannedLocalStorageHelper> helper(
      new CannedLocalStorageHelper(&context));

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin));
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedLocalStorageTest, Delete) {
  content::TestBrowserContext context;

  const GURL origin1("http://host1:9000");
  const GURL origin2("http://example.com");
  const GURL origin3("http://foo.example.com");

  scoped_refptr<CannedLocalStorageHelper> helper(
      new CannedLocalStorageHelper(&context));

  EXPECT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin1));
  helper->Add(url::Origin::Create(origin2));
  helper->Add(url::Origin::Create(origin3));
  EXPECT_EQ(3u, helper->GetCount());
  helper->DeleteOrigin(url::Origin::Create(origin2), base::DoNothing());
  EXPECT_EQ(2u, helper->GetCount());
  helper->DeleteOrigin(url::Origin::Create(origin1), base::DoNothing());
  EXPECT_EQ(1u, helper->GetCount());
}

TEST_F(CannedLocalStorageTest, IgnoreExtensionsAndDevTools) {
  content::TestBrowserContext context;

  const GURL origin1("chrome-extension://abcdefghijklmnopqrstuvwxyz/");
  const GURL origin2("devtools://abcdefghijklmnopqrstuvwxyz/");

  scoped_refptr<CannedLocalStorageHelper> helper(
      new CannedLocalStorageHelper(&context));

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin1));
  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin2));
  ASSERT_TRUE(helper->empty());
}

}  // namespace

}  // namespace browsing_data
