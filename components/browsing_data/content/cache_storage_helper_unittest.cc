// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/cache_storage_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browsing_data {
namespace {

class CannedCacheStorageHelperTest : public testing::Test {
 public:
  content::CacheStorageContext* CacheStorageContext() {
    return content::BrowserContext::GetDefaultStoragePartition(
               &browser_context_)
        ->GetCacheStorageContext();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(CannedCacheStorageHelperTest, Empty) {
  const GURL origin("http://host1:1/");

  scoped_refptr<CannedCacheStorageHelper> helper(
      new CannedCacheStorageHelper(CacheStorageContext()));

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin));
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedCacheStorageHelperTest, Delete) {
  const url::Origin origin1 = url::Origin::Create(GURL("http://host1:9000"));
  const url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));

  scoped_refptr<CannedCacheStorageHelper> helper(
      new CannedCacheStorageHelper(CacheStorageContext()));

  EXPECT_TRUE(helper->empty());
  helper->Add(origin1);
  helper->Add(origin2);
  helper->Add(origin2);
  EXPECT_EQ(2u, helper->GetCount());
  helper->DeleteCacheStorage(origin2);
  EXPECT_EQ(1u, helper->GetCount());
}

TEST_F(CannedCacheStorageHelperTest, IgnoreExtensionsAndDevTools) {
  const GURL origin1("chrome-extension://abcdefghijklmnopqrstuvwxyz/");
  const GURL origin2("devtools://abcdefghijklmnopqrstuvwxyz/");

  scoped_refptr<CannedCacheStorageHelper> helper(
      new CannedCacheStorageHelper(CacheStorageContext()));

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin1));
  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin2));
  ASSERT_TRUE(helper->empty());
}

}  // namespace
}  // namespace browsing_data
