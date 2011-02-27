// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"

#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/test_browser_window.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

namespace {

class TestTabContentsDelegate : public HtmlDialogTabContentsDelegate {
 public:
  explicit TestTabContentsDelegate(Profile* profile)
    : HtmlDialogTabContentsDelegate(profile) {}

  virtual ~TestTabContentsDelegate() {
  }

  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTabContentsDelegate);
};

class HtmlDialogTabContentsDelegateTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    test_tab_contents_delegate_.reset(new TestTabContentsDelegate(profile()));
  }

  virtual void TearDown() {
    test_tab_contents_delegate_.reset(NULL);
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  scoped_ptr<TestTabContentsDelegate> test_tab_contents_delegate_;
};

TEST_F(HtmlDialogTabContentsDelegateTest, DoNothingMethodsTest) {
  // None of the following calls should do anything.
  EXPECT_TRUE(test_tab_contents_delegate_->IsPopup(NULL));
  scoped_refptr<history::HistoryAddPageArgs> should_add_args(
      new history::HistoryAddPageArgs(
          GURL(), base::Time::Now(), 0, 0, GURL(), history::RedirectList(),
          PageTransition::TYPED, history::SOURCE_SYNCED, false));
  EXPECT_FALSE(test_tab_contents_delegate_->ShouldAddNavigationToHistory(
                   *should_add_args, NavigationType::NEW_PAGE));
  test_tab_contents_delegate_->NavigationStateChanged(NULL, 0);
  test_tab_contents_delegate_->ActivateContents(NULL);
  test_tab_contents_delegate_->LoadingStateChanged(NULL);
  test_tab_contents_delegate_->CloseContents(NULL);
  test_tab_contents_delegate_->UpdateTargetURL(NULL, GURL());
  test_tab_contents_delegate_->MoveContents(NULL, gfx::Rect());
  test_tab_contents_delegate_->ToolbarSizeChanged(NULL, false);
  EXPECT_EQ(0, browser()->tab_count());
  EXPECT_EQ(1U, BrowserList::size());
}

TEST_F(HtmlDialogTabContentsDelegateTest, OpenURLFromTabTest) {
  test_tab_contents_delegate_->OpenURLFromTab(
    NULL, GURL(chrome::kAboutBlankURL), GURL(),
    NEW_FOREGROUND_TAB, PageTransition::LINK);
  // This should create a new foreground tab in the existing browser.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1U, BrowserList::size());
}

TEST_F(HtmlDialogTabContentsDelegateTest, AddNewContentsForegroundTabTest) {
  TabContents* contents =
      new TabContents(profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  test_tab_contents_delegate_->AddNewContents(
      NULL, contents, NEW_FOREGROUND_TAB, gfx::Rect(), false);
  // This should create a new foreground tab in the existing browser.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1U, BrowserList::size());
}

TEST_F(HtmlDialogTabContentsDelegateTest, DetachTest) {
  EXPECT_EQ(profile(), test_tab_contents_delegate_->profile());
  test_tab_contents_delegate_->Detach();
  EXPECT_EQ(NULL, test_tab_contents_delegate_->profile());
  // Now, none of the following calls should do anything.
  test_tab_contents_delegate_->OpenURLFromTab(
      NULL, GURL(chrome::kAboutBlankURL), GURL(),
      NEW_FOREGROUND_TAB, PageTransition::LINK);
  test_tab_contents_delegate_->AddNewContents(NULL, NULL, NEW_FOREGROUND_TAB,
                                              gfx::Rect(), false);
  EXPECT_EQ(0, browser()->tab_count());
  EXPECT_EQ(1U, BrowserList::size());
}

}  // namespace
