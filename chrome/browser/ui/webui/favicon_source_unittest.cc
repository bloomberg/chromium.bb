// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include "base/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"

class FaviconSourceTest : public testing::Test {
 public:
  FaviconSourceTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, MessageLoop::current()),
        io_thread_(content::BrowserThread::IO, MessageLoop::current()),
        profile_(new TestingProfile()),
        favicon_source_(
            new FaviconSource(profile_.get(), FaviconSource::ANY)) {

    // Set the supported scale factors because the supported scale factors
    // affect the result of ParsePathAndScale().
    std::vector<ui::ScaleFactor> supported_scale_factors;
    supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
    supported_scale_factors.push_back(ui::SCALE_FACTOR_140P);
    scoped_set_supported_scale_factors_.reset(
        new ui::test::ScopedSetSupportedScaleFactors(supported_scale_factors));
  }

  virtual ~FaviconSourceTest() {
  }

  // Adds a most visited item with |url| and an arbitrary title to the instant
  // service and returns the assigned id.
  int AddInstantMostVisitedUrlAndGetId(GURL url) {
    InstantMostVisitedItem item;
    item.url = GURL(url);
    std::vector<InstantMostVisitedItem> items(1, item);

    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile_.get());
    instant_service->AddMostVisitedItems(items);

    std::vector<InstantMostVisitedItemIDPair> items_with_ids;
    instant_service->GetCurrentMostVisitedItems(&items_with_ids);
    CHECK_EQ(1u, items_with_ids.size());
    return items_with_ids[0].first;
  }

  FaviconSource* favicon_source() const { return favicon_source_.get(); }

 private:
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;

  typedef scoped_ptr<ui::test::ScopedSetSupportedScaleFactors>
      ScopedSetSupportedScaleFactors;
  ScopedSetSupportedScaleFactors scoped_set_supported_scale_factors_;

  scoped_ptr<FaviconSource> favicon_source_;

  DISALLOW_COPY_AND_ASSIGN(FaviconSourceTest);
};

// Test parsing the chrome-search://favicon/ URLs
TEST_F(FaviconSourceTest, InstantParsing) {
  GURL kUrl1("http://www.google.com/");
  GURL kUrl2("http://maps.google.com/");
  int instant_id1 = AddInstantMostVisitedUrlAndGetId(kUrl1);
  int instant_id2 = AddInstantMostVisitedUrlAndGetId(kUrl2);

  bool is_icon_url;
  GURL url;
  int size_in_dip;
  ui::ScaleFactor scale_factor;

  const std::string path1 = base::IntToString(instant_id1);
  EXPECT_TRUE(favicon_source()->ParsePath(path1, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ(kUrl1, url);
  EXPECT_EQ(16, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_100P, scale_factor);

  const std::string path2 = base::IntToString(instant_id2);
  EXPECT_TRUE(favicon_source()->ParsePath(path2, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ(kUrl2, url);
  EXPECT_EQ(16, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_100P, scale_factor);
}

// Test parsing the chrome://favicon URLs
TEST_F(FaviconSourceTest, Parsing) {
  const GURL kUrl("https://www.google.ca/imghp?hl=en&tab=wi");

  bool is_icon_url;
  GURL url;
  int size_in_dip;
  ui::ScaleFactor scale_factor;

  // 1) Test parsing path with no extra parameters.
  const std::string path1 = kUrl.spec();
  EXPECT_TRUE(favicon_source()->ParsePath(path1, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ(kUrl, url);
  EXPECT_EQ(16, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_100P, scale_factor);

  // 2) Test parsing path with a 'size' parameter.
  //
  // Test that we can still parse the legacy 'size' parameter format.
  const std::string path2 = "size/32/" + kUrl.spec();
  EXPECT_TRUE(favicon_source()->ParsePath(path2, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ(kUrl, url);
  EXPECT_EQ(32, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_100P, scale_factor);

  // Test parsing current 'size' parameter format.
  const std::string path3 = "size/32@1.4x/" + kUrl.spec();
  EXPECT_TRUE(favicon_source()->ParsePath(path3, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ(kUrl, url);
  EXPECT_EQ(32, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_140P, scale_factor);

  // Test that we pick the ui::ScaleFactor which is closest to the passed in
  // scale factor.
  const std::string path4 = "size/16@1.41x/" + kUrl.spec();
  EXPECT_TRUE(favicon_source()->ParsePath(path4, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ(kUrl, url);
  EXPECT_EQ(16, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_140P, scale_factor);

  // Invalid cases.
  const std::string path5 = "size/" + kUrl.spec();
  EXPECT_FALSE(favicon_source()->ParsePath(path5, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  const std::string path6 = "size/@1x/" + kUrl.spec();
  EXPECT_FALSE(favicon_source()->ParsePath(path6, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  const std::string path7 = "size/abc@1x/" + kUrl.spec();
  EXPECT_FALSE(favicon_source()->ParsePath(path7, &is_icon_url, &url,
      &size_in_dip, &scale_factor));

  // Part of url looks like 'size' parameter.
  const std::string path8 = "http://www.google.com/size/32@1.4x";
  EXPECT_TRUE(favicon_source()->ParsePath(path8, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ(path8, url.spec());
  EXPECT_EQ(16, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_100P, scale_factor);

  // 3) Test parsing path with the 'largest' parameter.
  const std::string path9 = "largest/" + kUrl.spec();
  EXPECT_TRUE(favicon_source()->ParsePath(path9, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ(kUrl, url);
  EXPECT_EQ(0, size_in_dip);
  // The scale factor is meaningless when requesting the largest favicon.

  // 4) Test parsing path with 'iconurl' parameter.
  const std::string path10 = "iconurl/http://www.google.com/favicon.ico";
  EXPECT_TRUE(favicon_source()->ParsePath(path10, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_TRUE(is_icon_url);
  EXPECT_EQ("http://www.google.com/favicon.ico", url.spec());
  EXPECT_EQ(16, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_100P, scale_factor);

  // 5) Test parsing path with 'origin' parameter.
  const std::string path11 = "origin/" + kUrl.spec();
  EXPECT_TRUE(favicon_source()->ParsePath(path11, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ("https://www.google.ca/", url.spec());
  EXPECT_EQ(16, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_100P, scale_factor);

  const std::string path12 = "origin/google.com";
  EXPECT_TRUE(favicon_source()->ParsePath(path12, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ("http://google.com/", url.spec());
  EXPECT_EQ(16, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_100P, scale_factor);

  // 6) Test parsing paths with both a 'size' parameter and a 'url modifier'
  // parameter.
  const std::string path13 = "size/32@1.4x/origin/" + kUrl.spec();
  EXPECT_TRUE(favicon_source()->ParsePath(path13, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_FALSE(is_icon_url);
  EXPECT_EQ("https://www.google.ca/", url.spec());
  EXPECT_EQ(32, size_in_dip);
  EXPECT_EQ(ui::SCALE_FACTOR_140P, scale_factor);

  const std::string path14 =
      "largest/iconurl/http://www.google.com/favicon.ico";
  EXPECT_TRUE(favicon_source()->ParsePath(path14, &is_icon_url, &url,
      &size_in_dip, &scale_factor));
  EXPECT_TRUE(is_icon_url);
  EXPECT_EQ("http://www.google.com/favicon.ico", url.spec());
  EXPECT_EQ(0, size_in_dip);
}
