// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/html_dialog_tab_contents_delegate.h"

#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/test_browser_window.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockTestBrowserWindow : public TestBrowserWindow {
 public:
  // TestBrowserWindow() doesn't actually use its browser argument so we just
  // pass NULL.
  MockTestBrowserWindow() : TestBrowserWindow(NULL) {}

  virtual ~MockTestBrowserWindow() {}

  MOCK_METHOD0(Show, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTestBrowserWindow);
};

class TestTabContentsDelegate : public HtmlDialogTabContentsDelegate {
 public:
  explicit TestTabContentsDelegate(Profile* profile)
    : HtmlDialogTabContentsDelegate(profile),
      window_for_next_created_browser_(NULL) {}

  virtual ~TestTabContentsDelegate() {
    CHECK(!window_for_next_created_browser_);
    for (std::vector<Browser*>::iterator i = created_browsers_.begin();
         i != created_browsers_.end(); ++i) {
      (*i)->CloseAllTabs();
      // We need to explicitly cast this since BrowserWindow does *not*
      // have a virtual destructor.
      delete static_cast<MockTestBrowserWindow*>((*i)->window());
      delete *i;
    }
  }

  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}

  // Takes ownership of window.
  void SetWindowForNextCreatedBrowser(BrowserWindow* window) {
    CHECK(window);
    window_for_next_created_browser_ = window;
  }

 protected:
  // CreateBrowser() is called by OpenURLFromTab() and AddNewContents().
  virtual Browser* CreateBrowser() {
    DCHECK(profile());
    DCHECK(window_for_next_created_browser_);
    Browser* browser = new Browser(Browser::TYPE_NORMAL, profile());
    browser->set_window(window_for_next_created_browser_);
    window_for_next_created_browser_ = NULL;
    created_browsers_.push_back(browser);
    return browser;
  }

 private:
  BrowserWindow* window_for_next_created_browser_;
  std::vector<Browser*> created_browsers_;

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
  EXPECT_FALSE(test_tab_contents_delegate_->ShouldAddNavigationToHistory());
  test_tab_contents_delegate_->NavigationStateChanged(NULL, 0);
  test_tab_contents_delegate_->ActivateContents(NULL);
  test_tab_contents_delegate_->LoadingStateChanged(NULL);
  test_tab_contents_delegate_->CloseContents(NULL);
  // URLStarredChanged() calls NOTREACHED() which immediately crashes.
  // Death tests take too long for unit_test tests on OS X and
  // there's http://code.google.com/p/chromium/issues/detail?id=24885 on
  // death tests on Windows so we simply don't call it.
  test_tab_contents_delegate_->UpdateTargetURL(NULL, GURL());
  test_tab_contents_delegate_->MoveContents(NULL, gfx::Rect());
  test_tab_contents_delegate_->ToolbarSizeChanged(NULL, false);
  EXPECT_EQ(0, browser()->tab_count());
  EXPECT_EQ(1U, BrowserList::size());
}

TEST_F(HtmlDialogTabContentsDelegateTest, OpenURLFromTabTest) {
  MockTestBrowserWindow* window = new MockTestBrowserWindow();
  EXPECT_CALL(*window, Show()).Times(1);
  test_tab_contents_delegate_->SetWindowForNextCreatedBrowser(window);

  test_tab_contents_delegate_->OpenURLFromTab(
    NULL, GURL(chrome::kAboutBlankURL), GURL(),
    NEW_FOREGROUND_TAB, PageTransition::LINK);
  EXPECT_EQ(0, browser()->tab_count());
  EXPECT_EQ(2U, BrowserList::size());
}

TEST_F(HtmlDialogTabContentsDelegateTest, AddNewContentsTest) {
  MockTestBrowserWindow* window = new MockTestBrowserWindow();
  EXPECT_CALL(*window, Show()).Times(1);
  test_tab_contents_delegate_->SetWindowForNextCreatedBrowser(window);

  TabContents* contents =
      new TabContents(profile(), NULL, MSG_ROUTING_NONE, NULL);
  test_tab_contents_delegate_->AddNewContents(
      NULL, contents, NEW_FOREGROUND_TAB, gfx::Rect(), false);
  EXPECT_EQ(0, browser()->tab_count());
  EXPECT_EQ(2U, BrowserList::size());
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

