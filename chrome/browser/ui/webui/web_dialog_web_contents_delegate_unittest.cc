// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

using content::OpenURLParams;
using content::Referrer;
using content::BrowserContext;
using content::WebContents;
using content::WebContentsTester;
using ui::WebDialogWebContentsDelegate;

namespace {

class TestWebContentsDelegate : public WebDialogWebContentsDelegate {
 public:
  explicit TestWebContentsDelegate(content::BrowserContext* context)
      : WebDialogWebContentsDelegate(context, new ChromeWebContentsHandler) {
  }
  virtual ~TestWebContentsDelegate() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebContentsDelegate);
};

class WebDialogWebContentsDelegateTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    test_web_contents_delegate_.reset(new TestWebContentsDelegate(profile()));
  }

  virtual void TearDown() {
    test_web_contents_delegate_.reset(NULL);
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  scoped_ptr<TestWebContentsDelegate> test_web_contents_delegate_;
};

TEST_F(WebDialogWebContentsDelegateTest, DoNothingMethodsTest) {
  // None of the following calls should do anything.
  EXPECT_TRUE(test_web_contents_delegate_->IsPopupOrPanel(NULL));
  history::HistoryAddPageArgs should_add_args(
      GURL(), base::Time::Now(), 0, 0, GURL(), history::RedirectList(),
      ui::PAGE_TRANSITION_TYPED, history::SOURCE_SYNCED, false);
  test_web_contents_delegate_->NavigationStateChanged(
      NULL, content::InvalidateTypes(0));
  test_web_contents_delegate_->ActivateContents(NULL);
  test_web_contents_delegate_->LoadingStateChanged(NULL, true);
  test_web_contents_delegate_->CloseContents(NULL);
  test_web_contents_delegate_->UpdateTargetURL(NULL, GURL());
  test_web_contents_delegate_->MoveContents(NULL, gfx::Rect());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}

TEST_F(WebDialogWebContentsDelegateTest, OpenURLFromTabTest) {
  test_web_contents_delegate_->OpenURLFromTab(
      NULL,
      OpenURLParams(GURL(url::kAboutBlankURL),
                    Referrer(),
                    NEW_FOREGROUND_TAB,
                    ui::PAGE_TRANSITION_LINK,
                    false));
  // This should create a new foreground tab in the existing browser.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}

TEST_F(WebDialogWebContentsDelegateTest, AddNewContentsForegroundTabTest) {
  WebContents* contents =
      WebContentsTester::CreateTestWebContents(profile(), NULL);
  test_web_contents_delegate_->AddNewContents(
      NULL, contents, NEW_FOREGROUND_TAB, gfx::Rect(), false, NULL);
  // This should create a new foreground tab in the existing browser.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}

TEST_F(WebDialogWebContentsDelegateTest, DetachTest) {
  EXPECT_EQ(profile(), test_web_contents_delegate_->browser_context());
  test_web_contents_delegate_->Detach();
  EXPECT_EQ(NULL, test_web_contents_delegate_->browser_context());
  // Now, none of the following calls should do anything.
  test_web_contents_delegate_->OpenURLFromTab(
      NULL,
      OpenURLParams(GURL(url::kAboutBlankURL),
                    Referrer(),
                    NEW_FOREGROUND_TAB,
                    ui::PAGE_TRANSITION_LINK,
                    false));
  test_web_contents_delegate_->AddNewContents(NULL, NULL, NEW_FOREGROUND_TAB,
                                              gfx::Rect(), false, NULL);
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}

}  // namespace
