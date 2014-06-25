// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestCompletionCallback {
 public:
  TestCompletionCallback()
      : have_result_(false) {
  }

  bool have_result() const { return have_result_; }

  void callback() {
    have_result_ = true;
  }

 private:
  bool have_result_;
};

}  // namespace

class CannedBrowsingDataAppCacheHelperTest : public testing::Test {
 public:
  CannedBrowsingDataAppCacheHelperTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD) {}

  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(CannedBrowsingDataAppCacheHelperTest, SetInfo) {
  TestingProfile profile;

  GURL manifest1("http://example1.com/manifest.xml");
  GURL manifest2("http://example2.com/path1/manifest.xml");
  GURL manifest3("http://example2.com/path2/manifest.xml");

  scoped_refptr<CannedBrowsingDataAppCacheHelper> helper(
      new CannedBrowsingDataAppCacheHelper(&profile));
  helper->AddAppCache(manifest1);
  helper->AddAppCache(manifest2);
  helper->AddAppCache(manifest3);

  TestCompletionCallback callback;
  helper->StartFetching(base::Bind(&TestCompletionCallback::callback,
                                   base::Unretained(&callback)));
  ASSERT_TRUE(callback.have_result());

  std::map<GURL, content::AppCacheInfoVector>& collection =
      helper->info_collection()->infos_by_origin;

  ASSERT_EQ(2u, collection.size());
  EXPECT_TRUE(ContainsKey(collection, manifest1.GetOrigin()));
  ASSERT_EQ(1u, collection[manifest1.GetOrigin()].size());
  EXPECT_EQ(manifest1, collection[manifest1.GetOrigin()].at(0).manifest_url);

  EXPECT_TRUE(ContainsKey(collection, manifest2.GetOrigin()));
  EXPECT_EQ(2u, collection[manifest2.GetOrigin()].size());
  std::set<GURL> manifest_results;
  manifest_results.insert(collection[manifest2.GetOrigin()].at(0).manifest_url);
  manifest_results.insert(collection[manifest2.GetOrigin()].at(1).manifest_url);
  EXPECT_TRUE(ContainsKey(manifest_results, manifest2));
  EXPECT_TRUE(ContainsKey(manifest_results, manifest3));
}

TEST_F(CannedBrowsingDataAppCacheHelperTest, Unique) {
  TestingProfile profile;

  GURL manifest("http://example.com/manifest.xml");

  scoped_refptr<CannedBrowsingDataAppCacheHelper> helper(
      new CannedBrowsingDataAppCacheHelper(&profile));
  helper->AddAppCache(manifest);
  helper->AddAppCache(manifest);

  TestCompletionCallback callback;
  helper->StartFetching(base::Bind(&TestCompletionCallback::callback,
                                   base::Unretained(&callback)));
  ASSERT_TRUE(callback.have_result());

  std::map<GURL, content::AppCacheInfoVector>& collection =
      helper->info_collection()->infos_by_origin;

  ASSERT_EQ(1u, collection.size());
  EXPECT_TRUE(ContainsKey(collection, manifest.GetOrigin()));
  ASSERT_EQ(1u, collection[manifest.GetOrigin()].size());
  EXPECT_EQ(manifest, collection[manifest.GetOrigin()].at(0).manifest_url);
}

TEST_F(CannedBrowsingDataAppCacheHelperTest, Empty) {
  TestingProfile profile;

  GURL manifest("http://example.com/manifest.xml");

  scoped_refptr<CannedBrowsingDataAppCacheHelper> helper(
      new CannedBrowsingDataAppCacheHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->AddAppCache(manifest);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedBrowsingDataAppCacheHelperTest, Delete) {
  TestingProfile profile;

  GURL manifest1("http://example.com/manifest1.xml");
  GURL manifest2("http://foo.example.com/manifest2.xml");
  GURL manifest3("http://bar.example.com/manifest3.xml");

  scoped_refptr<CannedBrowsingDataAppCacheHelper> helper(
      new CannedBrowsingDataAppCacheHelper(&profile));

  EXPECT_TRUE(helper->empty());
  helper->AddAppCache(manifest1);
  helper->AddAppCache(manifest2);
  helper->AddAppCache(manifest3);
  EXPECT_FALSE(helper->empty());
  EXPECT_EQ(3u, helper->GetAppCacheCount());
  helper->DeleteAppCacheGroup(manifest2);
  EXPECT_EQ(2u, helper->GetAppCacheCount());
  EXPECT_TRUE(helper->GetOriginAppCacheInfoMap().find(manifest2) ==
              helper->GetOriginAppCacheInfoMap().end());
}

TEST_F(CannedBrowsingDataAppCacheHelperTest, IgnoreExtensionsAndDevTools) {
  TestingProfile profile;

  GURL manifest1("chrome-extension://abcdefghijklmnopqrstuvwxyz/manifest.xml");
  GURL manifest2("chrome-devtools://abcdefghijklmnopqrstuvwxyz/manifest.xml");

  scoped_refptr<CannedBrowsingDataAppCacheHelper> helper(
      new CannedBrowsingDataAppCacheHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->AddAppCache(manifest1);
  ASSERT_TRUE(helper->empty());
  helper->AddAppCache(manifest2);
  ASSERT_TRUE(helper->empty());
}
