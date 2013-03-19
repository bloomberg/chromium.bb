// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/content_settings/content_setting_changed_infobar_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

class ContentSettingChangedInfoBarDelegateTest :
    public ChromeRenderViewHostTestHarness {
 public:
  ContentSettingChangedInfoBarDelegateTest() {
  }

  virtual ~ContentSettingChangedInfoBarDelegateTest() {
  }

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    InfoBarService::CreateForWebContents(web_contents());
  }
};

class InfoBarDelegateTestWebContentsObserver :
    public content::WebContentsObserver {
 public:
  explicit InfoBarDelegateTestWebContentsObserver(
      content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        navigation_count_(0) {
  }
  virtual ~InfoBarDelegateTestWebContentsObserver() {}

  virtual void NavigateToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE {
    ++navigation_count_;
  }

  int navigation_count() const { return navigation_count_; }

 private:
  int navigation_count_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarDelegateTestWebContentsObserver);
};

TEST_F(ContentSettingChangedInfoBarDelegateTest, InfoBar) {
  InfoBarDelegateTestWebContentsObserver observer(web_contents());

  content::WebContentsTester::For(web_contents())->NavigateAndCommit(
      GURL("data:,hello world"));

  const int icon = IDR_INFOBAR_MEDIA_STREAM_CAMERA;
  const int message = IDS_MEDIASTREAM_SETTING_CHANGED_INFOBAR_MESSAGE;

  scoped_ptr<ContentSettingChangedInfoBarDelegate> infobar(
      ContentSettingChangedInfoBarDelegate::CreateForUnitTest(
          InfoBarService::FromWebContents(web_contents()),
          icon,
          message));
  ConfirmInfoBarDelegate* c_infobar = infobar.get();

  EXPECT_EQ(&ResourceBundle::GetSharedInstance().GetNativeImageNamed(icon),
            c_infobar->GetIcon());
  EXPECT_FALSE(c_infobar->GetMessageText().compare(
      l10n_util::GetStringUTF16(message)));

  EXPECT_EQ(1, observer.navigation_count());
  EXPECT_TRUE(c_infobar->Accept());
  EXPECT_EQ(2, observer.navigation_count());
}
