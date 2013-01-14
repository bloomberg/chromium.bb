// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_path.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/ui/ash/launcher/browser_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_favicon_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/favicon_url.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Observer class to determine when favicons have completed loading.
class ContentsObserver : public content::WebContentsObserver {
 public:
  explicit ContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        got_favicons_(false) {
  }

  virtual ~ContentsObserver() {}

  bool got_favicons() const { return got_favicons_; }
  void Reset() {
    got_favicons_ = false;
  }

  // content::WebContentsObserver overrides.
  virtual void DidUpdateFaviconURL(int32 page_id,
      const std::vector<content::FaviconURL>& candidates) OVERRIDE {
    if (!candidates.empty())
      got_favicons_ = true;
  }

 private:
  bool got_favicons_;
};

}  // namespace

class LauncherFaviconLoaderBrowsertest : public InProcessBrowserTest {
 public:
  LauncherFaviconLoaderBrowsertest()
    : panel_browser_(NULL),
      loader_(NULL),
      contents_observer_(NULL) {
  }

  virtual ~LauncherFaviconLoaderBrowsertest() {
  }

 protected:
  void NavigateTo(const char* url) {
    Browser* browser = GetPanelBrowser();
    std::string url_path = base::StringPrintf("files/ash/launcher/%s", url);
    ui_test_utils::NavigateToURL(browser, test_server()->GetURL(url_path));
  }

  Browser* GetPanelBrowser() {
    if (!panel_browser_) {
      panel_browser_ = new Browser(Browser::CreateParams::CreateForApp(
          Browser::TYPE_PANEL, "Test Panel", gfx::Rect(),
          browser()->profile()));
      EXPECT_TRUE(panel_browser_->is_type_panel());
      // Load initial web contents before setting the observer.
      ui_test_utils::NavigateToURL(panel_browser_, GURL());
      EXPECT_FALSE(contents_observer_.get());
      contents_observer_.reset(
          new ContentsObserver(
              panel_browser_->tab_strip_model()->GetWebContentsAt(0)));
    }
    return panel_browser_;
  }

  LauncherFaviconLoader* GetFaviconLoader() {
    if (!loader_) {
      Browser* browser = GetPanelBrowser();
      BrowserView* browser_view = static_cast<BrowserView*>(browser->window());
      BrowserLauncherItemController* launcher_item_controller =
          browser_view->launcher_item_controller();
      if (!launcher_item_controller)
        return NULL;
      EXPECT_EQ(BrowserLauncherItemController::TYPE_EXTENSION_PANEL,
                launcher_item_controller->type());
      loader_ = launcher_item_controller->favicon_loader();
    }
    return loader_;
  }

  void ResetDownloads() {
    ASSERT_NE(static_cast<void*>(NULL), contents_observer_.get());
    contents_observer_->Reset();
  }

  bool WaitForFaviconDownloads() {
    LauncherFaviconLoader* loader = GetFaviconLoader();
    if (!loader)
      return false;

    const int64 max_seconds = 2;
    base::Time start_time = base::Time::Now();
    while (!contents_observer_->got_favicons() ||
           loader->HasPendingDownloads()) {
      content::RunAllPendingInMessageLoop();
      base::TimeDelta delta = base::Time::Now() - start_time;
      if (delta.InSeconds() >= max_seconds) {
        LOG(ERROR) << " WaitForFaviconDownlads timed out:"
                   << " Got Favicons:" << contents_observer_->got_favicons()
                   << " Pending Downloads: " << loader->HasPendingDownloads();
        return false;
      }
    }
    return true;
  }

 private:
  Browser* panel_browser_;
  LauncherFaviconLoader* loader_;
  scoped_ptr<ContentsObserver> contents_observer_;
};

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, SmallLauncherIcon) {
  ASSERT_TRUE(test_server()->Start());
  NavigateTo("launcher-smallfavicon.html");
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader();
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);
  EXPECT_TRUE(WaitForFaviconDownloads());
  // No large favicons specified, bitmap should be empty.
  EXPECT_TRUE(favicon_loader->GetFavicon().empty());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, LargeLauncherIcon) {
  ASSERT_TRUE(test_server()->Start());
  NavigateTo("launcher-largefavicon.html");
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader();
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);
  EXPECT_TRUE(WaitForFaviconDownloads());
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  EXPECT_EQ(128, favicon_loader->GetFavicon().height());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, ManyLauncherIcons) {
  ASSERT_TRUE(test_server()->Start());
  NavigateTo("launcher-manyfavicon.html");
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader();
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);

  EXPECT_TRUE(WaitForFaviconDownloads());
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  // When multiple favicons are present, the correctly sized icon should be
  // chosen. The icons are sized assuming ash::kLauncherPreferredSize < 128.
  EXPECT_GT(128, ash::kLauncherPreferredSize);
  EXPECT_EQ(48, favicon_loader->GetFavicon().height());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, ChangeLauncherIcons) {
  ASSERT_TRUE(test_server()->Start());
  NavigateTo("launcher-manyfavicon.html");
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader();
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);

  EXPECT_TRUE(WaitForFaviconDownloads());
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  EXPECT_EQ(48, favicon_loader->GetFavicon().height());
  ASSERT_NO_FATAL_FAILURE(ResetDownloads());

  NavigateTo("launcher-smallfavicon.html");
  EXPECT_TRUE(WaitForFaviconDownloads());
  EXPECT_TRUE(favicon_loader->GetFavicon().empty());
  ASSERT_NO_FATAL_FAILURE(ResetDownloads());

  NavigateTo("launcher-largefavicon.html");
  EXPECT_TRUE(WaitForFaviconDownloads());
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  EXPECT_EQ(128, favicon_loader->GetFavicon().height());
}
