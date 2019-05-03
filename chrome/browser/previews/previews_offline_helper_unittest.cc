// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_offline_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/testing_pref_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "testing/gtest/include/gtest/gtest.h"

class PreviewsOfflineHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void TearDown() override {
    if (helper_)
      helper_->Shutdown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PreviewsOfflineHelper* NewHelper() {
    helper_.reset(new PreviewsOfflineHelper(browser_context()));
    return helper_.get();
  }

  offline_pages::OfflinePageItem MakeAddedPageItem(
      const std::string& url,
      const std::string& original_url,
      const base::Time& creation_time) {
    offline_pages::OfflinePageItem item;
    item.url = GURL(url);
    item.original_url_if_different = GURL(original_url);
    item.creation_time = creation_time;
    return item;
  }

  offline_pages::OfflinePageItem MakeDeletedPageItem(const std::string& url) {
    offline_pages::OfflinePageItem item;
    item.url = GURL(url);  // Only |url| is needed.
    return item;
  }

 private:
  std::unique_ptr<PreviewsOfflineHelper> helper_;
};

TEST_F(PreviewsOfflineHelperTest, TestAddRemovePages) {
  struct TestCase {
    std::string msg;
    bool enable_feature;
    std::vector<std::string> add_fresh_pages;
    std::vector<std::string> add_expired_pages;
    std::vector<std::string> delete_pages;
    std::vector<std::string> want_pages;
    std::vector<std::string> not_want_pages;
    std::string original_url;
    size_t want_pref_size;
  };
  const TestCase kTestCases[]{
      {
          .msg = "All pages should return true when the feature is disabled",
          .enable_feature = false,
          .add_fresh_pages = {},
          .add_expired_pages = {},
          .delete_pages = {},
          .want_pages = {"http://chromium.org"},
          .not_want_pages = {},
          .original_url = "",
          .want_pref_size = 0,
      },
      {
          .msg = "Unknown page returns false",
          .enable_feature = true,
          .add_fresh_pages = {},
          .add_expired_pages = {},
          .delete_pages = {},
          .want_pages = {},
          .not_want_pages = {"http://chromium.org"},
          .original_url = "",
          .want_pref_size = 0,
      },
      {
          .msg = "Fresh page returns true",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org"},
          .add_expired_pages = {},
          .delete_pages = {},
          .want_pages = {"http://chromium.org"},
          .not_want_pages = {},
          .original_url = "",
          .want_pref_size = 1,
      },
      {
          .msg = "Fresh page with the original URL returns true",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org"},
          .add_expired_pages = {},
          .delete_pages = {},
          .want_pages = {"http://google.com"},
          .not_want_pages = {},
          .original_url = "http://google.com",
          .want_pref_size = 2,
      },
      {
          .msg = "Expired page returns false",
          .enable_feature = true,
          .add_fresh_pages = {},
          .add_expired_pages = {"http://chromium.org"},
          .delete_pages = {},
          .want_pages = {},
          .not_want_pages = {"http://chromium.org"},
          .original_url = "",
          .want_pref_size = 0,
      },
      {
          .msg = "Added then deleted page returns false",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org"},
          .add_expired_pages = {},
          .delete_pages = {"http://chromium.org"},
          .want_pages = {},
          .not_want_pages = {"http://chromium.org"},
          .original_url = "",
          .want_pref_size = 0,
      },
      {
          .msg = "Expired then refreshed page returns true",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org"},
          .add_expired_pages = {"http://chromium.org"},
          .delete_pages = {},
          .want_pages = {"http://chromium.org"},
          .not_want_pages = {},
          .original_url = "",
          .want_pref_size = 1,
      },
      {
          .msg = "URL Fragments don't matter",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org"},
          .add_expired_pages = {},
          .delete_pages = {},
          .want_pages = {"http://chromium.org",
                         "http://chromium.org/#previews"},
          .not_want_pages = {},
          .original_url = "",
          .want_pref_size = 1,
      },
      {
          .msg = "URLs with paths are different",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org/fresh",
                              "http://chromium.org/fresh_but_deleted"},
          .add_expired_pages = {"http://chromium.org/old"},
          .delete_pages = {"http://chromium.org/fresh_but_deleted"},
          .want_pages = {"http://chromium.org/fresh"},
          .not_want_pages = {"http://chromium.org/old",
                             "http://chromium.org/fresh_but_deleted"},
          .original_url = "",
          .want_pref_size = 1,
      },
  };

  base::Time fresh = base::Time::Now();
  base::Time expired = fresh -
                       previews::params::OfflinePreviewFreshnessDuration() -
                       base::TimeDelta::FromHours(1);

  const char kDictKey[] = "previews.offline_helper.available_pages";

  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.msg);

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        previews::features::kOfflinePreviewsFalsePositivePrevention,
        test_case.enable_feature);

    TestingPrefServiceSimple test_prefs;
    PreviewsOfflineHelper::RegisterProfilePrefs(test_prefs.registry());

    PreviewsOfflineHelper* helper = NewHelper();
    helper->SetPrefServiceForTesting(&test_prefs);

    // The tests above rely on this ordering.
    for (const std::string& expired_page : test_case.add_expired_pages) {
      helper->OfflinePageAdded(
          nullptr,
          MakeAddedPageItem(expired_page, test_case.original_url, expired));
    }
    for (const std::string& fresh_page : test_case.add_fresh_pages) {
      helper->OfflinePageAdded(
          nullptr,
          MakeAddedPageItem(fresh_page, test_case.original_url, fresh));
    }
    for (const std::string& deleted_page : test_case.delete_pages) {
      helper->OfflinePageDeleted(MakeDeletedPageItem(deleted_page));
    }

    EXPECT_EQ(test_prefs.GetDictionary(kDictKey)->size(),
              test_case.want_pref_size);

    for (const std::string want : test_case.want_pages) {
      EXPECT_TRUE(helper->ShouldAttemptOfflinePreview(GURL(want)));
    }

    for (const std::string not_want : test_case.not_want_pages) {
      EXPECT_FALSE(helper->ShouldAttemptOfflinePreview(GURL(not_want)));
    }
  }
}

TEST_F(PreviewsOfflineHelperTest, TestMaxPrefSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      previews::features::kOfflinePreviewsFalsePositivePrevention,
      {{"max_pref_entries", "1"}});

  PreviewsOfflineHelper* helper = NewHelper();

  base::Time first = base::Time::Now();
  base::Time second = first + base::TimeDelta::FromMinutes(1);

  helper->OfflinePageAdded(
      nullptr, MakeAddedPageItem("http://test.first.com", "", first));
  EXPECT_TRUE(
      helper->ShouldAttemptOfflinePreview(GURL("http://test.first.com")));

  helper->OfflinePageAdded(
      nullptr, MakeAddedPageItem("http://test.second.com", "", second));
  EXPECT_FALSE(
      helper->ShouldAttemptOfflinePreview(GURL("http://test.first.com")));
  EXPECT_TRUE(
      helper->ShouldAttemptOfflinePreview(GURL("http://test.second.com")));
}
