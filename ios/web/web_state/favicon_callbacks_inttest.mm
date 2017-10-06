// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "ios/web/public/favicon_url.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "ios/web/public/web_state/web_state_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// Observes and waits for FaviconUrlUpdated call.
class FaviconUrlObserver : public web::WebStateObserver {
 public:
  explicit FaviconUrlObserver(web::WebState* web_state)
      : web::WebStateObserver(web_state) {}
  // Returns vavicon url candidates received in FaviconUrlUpdated.
  const std::vector<FaviconURL>& favicon_url_candidates() const {
    return favicon_url_candidates_;
  }
  // Returns true if FaviconUrlUpdated was called.
  bool favicon_url_updated() const { return favicon_url_updated_; }
  // WebStateObserver overrides:
  void FaviconUrlUpdated(const std::vector<FaviconURL>& candidates) override {
    favicon_url_candidates_ = candidates;
    favicon_url_updated_ = true;
  }

 private:
  bool favicon_url_updated_ = false;
  std::vector<FaviconURL> favicon_url_candidates_;
};

}  // namespace

// Test fixture for WebStateDelegate::FaviconUrlUpdated and integration tests.
class FaviconCallbackTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    observer_ = base::MakeUnique<FaviconUrlObserver>(web_state());
  }
  std::unique_ptr<FaviconUrlObserver> observer_;
};

// Tests page with shortcut icon link.
TEST_F(FaviconCallbackTest, ShortcutIconFavicon) {
  ASSERT_TRUE(observer_->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='shortcut icon' href='http://fav.ico'>");

  WaitForCondition(^{
    return observer_->favicon_url_updated();
  });

  const std::vector<FaviconURL>& favicons = observer_->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::FAVICON, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
};

// Tests page with icon link.
TEST_F(FaviconCallbackTest, IconFavicon) {
  ASSERT_TRUE(observer_->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='icon' href='http://fav.ico'>");

  WaitForCondition(^{
    return observer_->favicon_url_updated();
  });

  const std::vector<FaviconURL>& favicons = observer_->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::FAVICON, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
};

// Tests page with apple-touch-icon link.
TEST_F(FaviconCallbackTest, AppleTouchIconFavicon) {
  ASSERT_TRUE(observer_->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='apple-touch-icon' href='http://fav.ico'>",
           GURL("https://chromium.test"));

  WaitForCondition(^{
    return observer_->favicon_url_updated();
  });

  const std::vector<FaviconURL>& favicons = observer_->favicon_url_candidates();
  ASSERT_EQ(2U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::TOUCH_ICON, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"), favicons[1].icon_url);
  EXPECT_EQ(FaviconURL::FAVICON, favicons[1].icon_type);
  ASSERT_TRUE(favicons[1].icon_sizes.empty());
};

// Tests page with apple-touch-icon-precomposed link.
TEST_F(FaviconCallbackTest, AppleTouchIconPrecomposedFavicon) {
  ASSERT_TRUE(observer_->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='apple-touch-icon-precomposed' href='http://fav.ico'>",
           GURL("https://chromium.test"));

  WaitForCondition(^{
    return observer_->favicon_url_updated();
  });

  const std::vector<FaviconURL>& favicons = observer_->favicon_url_candidates();
  ASSERT_EQ(2U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::TOUCH_PRECOMPOSED_ICON, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"), favicons[1].icon_url);
  EXPECT_EQ(FaviconURL::FAVICON, favicons[1].icon_type);
  ASSERT_TRUE(favicons[1].icon_sizes.empty());
};

// Tests page without favicon link.
TEST_F(FaviconCallbackTest, NoFavicon) {
  ASSERT_TRUE(observer_->favicon_url_candidates().empty());
  LoadHtml(@"<html></html>", GURL("https://chromium.test/test/test.html"));

  WaitForCondition(^{
    return observer_->favicon_url_updated();
  });

  const std::vector<FaviconURL>& favicons = observer_->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::FAVICON, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
};

// Tests page with multiple favicon links.
TEST_F(FaviconCallbackTest, MultipleFavicons) {
  ASSERT_TRUE(observer_->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='shortcut icon' href='http://fav.ico'>"
            "<link rel='icon' href='http://fav1.ico'>"
            "<link rel='apple-touch-icon' href='http://fav2.ico'>"
            "<link rel='apple-touch-icon-precomposed' href='http://fav3.ico'>");

  WaitForCondition(^{
    return observer_->favicon_url_updated();
  });

  const std::vector<FaviconURL>& favicons = observer_->favicon_url_candidates();
  ASSERT_EQ(4U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::FAVICON, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
  EXPECT_EQ(GURL("http://fav1.ico"), favicons[1].icon_url);
  EXPECT_EQ(FaviconURL::FAVICON, favicons[1].icon_type);
  ASSERT_TRUE(favicons[1].icon_sizes.empty());
  EXPECT_EQ(GURL("http://fav2.ico"), favicons[2].icon_url);
  EXPECT_EQ(FaviconURL::TOUCH_ICON, favicons[2].icon_type);
  ASSERT_TRUE(favicons[2].icon_sizes.empty());
  EXPECT_EQ(GURL("http://fav3.ico"), favicons[3].icon_url);
  EXPECT_EQ(FaviconURL::TOUCH_PRECOMPOSED_ICON, favicons[3].icon_type);
  ASSERT_TRUE(favicons[3].icon_sizes.empty());
};

// Tests page with invalid favicon url.
TEST_F(FaviconCallbackTest, InvalidFaviconUrl) {
  ASSERT_TRUE(observer_->favicon_url_candidates().empty());
  LoadHtml(@"<html><head><link rel='icon' href='http://'></head></html>",
           GURL("https://chromium.test"));

  WaitForCondition(^{
    return observer_->favicon_url_updated();
  });

  const std::vector<FaviconURL>& favicons = observer_->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::FAVICON, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
};

// Tests page with empty favicon url.
TEST_F(FaviconCallbackTest, EmptyFaviconUrl) {
  ASSERT_TRUE(observer_->favicon_url_candidates().empty());
  LoadHtml(@"<head><link rel='icon' href=''></head>");

  WaitForCondition(^{
    return observer_->favicon_url_updated();
  });

  const std::vector<FaviconURL>& favicons = observer_->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  // TODO(crbug.com/721852): This result is not correct.
  EXPECT_EQ(GURL("https://chromium.test/"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::FAVICON, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
};

}  // namespace web
