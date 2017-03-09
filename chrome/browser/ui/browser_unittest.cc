// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "third_party/skia/include/core/SkColor.h"

using content::SiteInstance;
using content::WebContents;
using content::WebContentsTester;

class BrowserUnitTest : public BrowserWithTestWindowTest {
 public:
  BrowserUnitTest() {}
  ~BrowserUnitTest() override {}

  // Caller owns the memory.
  WebContents* CreateTestWebContents() {
    return WebContentsTester::CreateTestWebContents(
        profile(), SiteInstance::Create(profile()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserUnitTest);
};

// Ensure crashed tabs are not reloaded when selected. crbug.com/232323
TEST_F(BrowserUnitTest, ReloadCrashedTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Start with a single foreground tab. |tab_strip_model| owns the memory.
  WebContents* contents1 = CreateTestWebContents();
  tab_strip_model->AppendWebContents(contents1, true);
  WebContentsTester::For(contents1)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(contents1)->TestSetIsLoading(false);
  EXPECT_TRUE(tab_strip_model->IsTabSelected(0));
  EXPECT_FALSE(contents1->IsLoading());

  // Add a second tab in the background.
  WebContents* contents2 = CreateTestWebContents();
  tab_strip_model->AppendWebContents(contents2, false);
  WebContentsTester::For(contents2)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(contents2)->TestSetIsLoading(false);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(tab_strip_model->IsTabSelected(0));
  EXPECT_FALSE(contents2->IsLoading());

  // Simulate the second tab crashing.
  contents2->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_TRUE(contents2->IsCrashed());

  // Selecting the second tab does not cause a load or clear the crash.
  tab_strip_model->ActivateTabAt(1, true);
  EXPECT_TRUE(tab_strip_model->IsTabSelected(1));
  EXPECT_FALSE(contents2->IsLoading());
  EXPECT_TRUE(contents2->IsCrashed());
}

TEST_F(BrowserUnitTest, SetBackgroundColorForNewTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  WebContents* contents1 = CreateTestWebContents();
  tab_strip_model->AppendWebContents(contents1, true);
  WebContentsTester::For(contents1)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(contents1)->TestSetIsLoading(false);

  contents1->GetMainFrame()->GetView()->SetBackgroundColor(SK_ColorRED);

  // Add a second tab in the background.
  WebContents* contents2 = CreateTestWebContents();
  tab_strip_model->AppendWebContents(contents2, false);
  WebContentsTester::For(contents2)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(contents2)->TestSetIsLoading(false);

  tab_strip_model->ActivateTabAt(1, true);
  EXPECT_EQ(SK_ColorRED,
            contents2->GetMainFrame()->GetView()->background_color());
}

// Ensure the print command gets disabled when a tab crashes.
TEST_F(BrowserUnitTest, DisablePrintOnCrashedTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  WebContents* contents = CreateTestWebContents();
  tab_strip_model->AppendWebContents(contents, true);
  WebContentsTester::For(contents)->NavigateAndCommit(GURL("about:blank"));

  CommandUpdater* command_updater =
      browser()->command_controller()->command_updater();

  EXPECT_FALSE(contents->IsCrashed());
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_PRINT));
  EXPECT_TRUE(chrome::CanPrint(browser()));

  contents->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);

  EXPECT_TRUE(contents->IsCrashed());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_PRINT));
  EXPECT_FALSE(chrome::CanPrint(browser()));
}

// Ensure the zoom-in and zoom-out commands get disabled when a tab crashes.
TEST_F(BrowserUnitTest, DisableZoomOnCrashedTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  WebContents* contents = CreateTestWebContents();
  tab_strip_model->AppendWebContents(contents, true);
  WebContentsTester::For(contents)->NavigateAndCommit(GURL("about:blank"));
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(contents);
  EXPECT_TRUE(zoom_controller->SetZoomLevel(zoom_controller->
                                            GetDefaultZoomLevel()));

  CommandUpdater* command_updater =
      browser()->command_controller()->command_updater();

  EXPECT_TRUE(zoom_controller->IsAtDefaultZoom());
  EXPECT_FALSE(contents->IsCrashed());
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_ZOOM_PLUS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_ZOOM_MINUS));
  EXPECT_TRUE(chrome::CanZoomIn(contents));
  EXPECT_TRUE(chrome::CanZoomOut(contents));

  contents->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);

  EXPECT_TRUE(contents->IsCrashed());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_ZOOM_PLUS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_ZOOM_MINUS));
  EXPECT_FALSE(chrome::CanZoomIn(contents));
  EXPECT_FALSE(chrome::CanZoomOut(contents));
}

class BrowserBookmarkBarTest : public BrowserWithTestWindowTest {
 public:
  BrowserBookmarkBarTest() {}
  ~BrowserBookmarkBarTest() override {}

 protected:
  BookmarkBar::State window_bookmark_bar_state() const {
    return static_cast<BookmarkBarStateTestBrowserWindow*>(
        browser()->window())->bookmark_bar_state();
  }

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    static_cast<BookmarkBarStateTestBrowserWindow*>(
        browser()->window())->set_browser(browser());
  }

  BrowserWindow* CreateBrowserWindow() override {
    return new BookmarkBarStateTestBrowserWindow();
  }

 private:
  class BookmarkBarStateTestBrowserWindow : public TestBrowserWindow {
   public:
    BookmarkBarStateTestBrowserWindow()
        : browser_(NULL),
          bookmark_bar_state_(BookmarkBar::HIDDEN) {}
    ~BookmarkBarStateTestBrowserWindow() override {}

    void set_browser(Browser* browser) { browser_ = browser; }

    BookmarkBar::State bookmark_bar_state() const {
      return bookmark_bar_state_;
    }

   private:
    // TestBrowserWindow:
    void BookmarkBarStateChanged(
        BookmarkBar::AnimateChangeType change_type) override {
      bookmark_bar_state_ = browser_->bookmark_bar_state();
      TestBrowserWindow::BookmarkBarStateChanged(change_type);
    }

    void OnActiveTabChanged(content::WebContents* old_contents,
                            content::WebContents* new_contents,
                            int index,
                            int reason) override {
      bookmark_bar_state_ = browser_->bookmark_bar_state();
      TestBrowserWindow::OnActiveTabChanged(old_contents, new_contents, index,
                                            reason);
    }

    Browser* browser_;  // Weak ptr.
    BookmarkBar::State bookmark_bar_state_;

    DISALLOW_COPY_AND_ASSIGN(BookmarkBarStateTestBrowserWindow);
  };

  DISALLOW_COPY_AND_ASSIGN(BrowserBookmarkBarTest);
};

// Ensure bookmark bar states in Browser and BrowserWindow are in sync after
// Browser::ActiveTabChanged() calls BrowserWindow::OnActiveTabChanged().
TEST_F(BrowserBookmarkBarTest, StateOnActiveTabChanged) {
  ASSERT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  ASSERT_EQ(BookmarkBar::HIDDEN, window_bookmark_bar_state());

  GURL ntp_url("chrome://newtab");
  GURL non_ntp_url("http://foo");

  // Open a tab to NTP.
  AddTab(browser(), ntp_url);
  EXPECT_EQ(BookmarkBar::DETACHED, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::DETACHED, window_bookmark_bar_state());

  // Navigate 1st tab to a non-NTP URL.
  NavigateAndCommitActiveTab(non_ntp_url);
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::HIDDEN, window_bookmark_bar_state());

  // Open a tab to NTP at index 0.
  AddTab(browser(), ntp_url);
  EXPECT_EQ(BookmarkBar::DETACHED, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::DETACHED, window_bookmark_bar_state());

  // Activate the 2nd tab which is non-NTP.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::HIDDEN, window_bookmark_bar_state());

  // Toggle bookmark bar while 2nd tab (non-NTP) is active.
  chrome::ToggleBookmarkBar(browser());
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::SHOW, window_bookmark_bar_state());

  // Activate the 1st tab which is NTP.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::SHOW, window_bookmark_bar_state());

  // Activate the 2nd tab which is non-NTP.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::SHOW, window_bookmark_bar_state());
}
