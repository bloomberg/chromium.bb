// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"

class TestInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  static bool Create(InfoBarService* infobar_service) {
    return !!infobar_service->AddInfoBar(
        std::make_unique<InfoBarView>(std::make_unique<TestInfoBarDelegate>()));
  }

  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }
};

class InfoBarViewTest : public ChromeViewsTestBase {
 public:
  InfoBarViewTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&profile_)),
        infobar_container_view_(nullptr) {
    InfoBarService::CreateForWebContents(web_contents_);
    infobar_container_view_.ChangeInfoBarManager(infobar_service());
  }

  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents_);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  content::WebContents* web_contents_;
  InfoBarContainerView infobar_container_view_;
};

TEST_F(InfoBarViewTest, ShouldDrawSeparator) {
  // Add multiple infobars.
  for (int i = 0; i < 3; ++i)
    EXPECT_TRUE(TestInfoBarDelegate::Create(infobar_service()));

  // The top infobar should not draw a separator; the others should.
  auto infobar_at = [this](size_t index) {
    return static_cast<InfoBarView*>(infobar_service()->infobar_at(index));
  };
  EXPECT_FALSE(infobar_at(0)->ShouldDrawSeparator());
  EXPECT_TRUE(infobar_at(1)->ShouldDrawSeparator());
  EXPECT_TRUE(infobar_at(2)->ShouldDrawSeparator());
}
