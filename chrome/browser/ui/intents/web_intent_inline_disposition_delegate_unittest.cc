// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebIntentInlineDispositionBrowserTest
    : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();

    content::WebContents* contents = content::WebContents::Create(
        browser()->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
    wrapper_.reset(new TabContentsWrapper(contents));
    delegate_.reset(new WebIntentInlineDispositionDelegate);
    contents->SetDelegate(delegate_.get());
  }

 protected:
  TestingProfile profile_;
  scoped_ptr<TabContentsWrapper> wrapper_;
  scoped_ptr<WebIntentInlineDispositionDelegate> delegate_;
};

// Verifies delegate's OpenURLFromTab works. This allows navigation inside
// web intents picker.
TEST_F(WebIntentInlineDispositionBrowserTest, OpenURLFromTabTest) {
  content::WebContents* source = delegate_->OpenURLFromTab(
    wrapper_->web_contents(),
    content::OpenURLParams(GURL(chrome::kAboutBlankURL), content::Referrer(),
    NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK, false));

  // Ensure OpenURLFromTab loaded about::blank in |web_contents_|.
  ASSERT_EQ(wrapper_->web_contents(), source);
  EXPECT_EQ(GURL(chrome::kAboutBlankURL).spec(), source->GetURL().spec());
}
