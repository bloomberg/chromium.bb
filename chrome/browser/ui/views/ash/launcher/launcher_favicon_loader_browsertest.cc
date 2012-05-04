// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_path.h"
#include "base/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/ash/launcher/browser_launcher_item_controller.h"
#include "chrome/browser/ui/views/ash/launcher/launcher_favicon_loader.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/favicon_url.h"
#include "chrome/common/icon_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Observer class to determine when favicons have completed loading.
class ContentsObserver : public content::WebContentsObserver {
 public:
  explicit ContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        got_favicons_(false),
        downloads_received_(0) {
  }

  virtual ~ContentsObserver() {}

  bool got_favicons() const { return got_favicons_; }
  int downloads_received() const { return downloads_received_; }
  void Reset() {
    got_favicons_ = false;
    downloads_received_ = 0;
  }

  // content::WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool message_handled = false;   // Allow other handlers to receive these.
    IPC_BEGIN_MESSAGE_MAP(ContentsObserver, message)
      IPC_MESSAGE_HANDLER(IconHostMsg_DidDownloadFavicon, OnDidDownloadFavicon)
      IPC_MESSAGE_HANDLER(IconHostMsg_UpdateFaviconURL, OnUpdateFaviconURL)
      IPC_MESSAGE_UNHANDLED(message_handled = false)
    IPC_END_MESSAGE_MAP()
    return message_handled;
  }

 private:
  void OnUpdateFaviconURL(int32 page_id,
                          const std::vector<FaviconURL>& candidates) {
    got_favicons_ = true;
  }

  void OnDidDownloadFavicon(int id,
                            const GURL& image_url,
                            bool errored,
                            const SkBitmap& bitmap) {
    ++downloads_received_;
  }

  bool got_favicons_;
  int downloads_received_;
};

}  // namespace

class LauncherFaviconLoaderBrowsertest : public InProcessBrowserTest {
 protected:
  void NavigateTo(Browser* browser, const char* url) {
    std::string url_path = base::StringPrintf("files/ash/launcher/%s", url);
    ui_test_utils::NavigateToURL(browser, test_server()->GetURL(url_path));
  }

  void CreatePanelBrowser(const char* url, Browser** result) {
    Browser* panel_browser =  Browser::CreateWithParams(
        Browser::CreateParams::CreateForApp(
            Browser::TYPE_PANEL, "Test Panel", gfx::Rect(),
            browser()->profile()));
    EXPECT_TRUE(panel_browser->is_type_panel());
    ASSERT_EQ(static_cast<void*>(NULL), contents_observer_.get());
    // Load initial tab contents before setting the observer.
    ui_test_utils::NavigateToURL(panel_browser, GURL());
    contents_observer_.reset(
        new ContentsObserver(panel_browser->GetWebContentsAt(0)));
    NavigateTo(panel_browser, url);
    *result = panel_browser;
  }

  LauncherFaviconLoader* GetFaviconLoader(Browser* browser) {
    BrowserView* browser_view = static_cast<BrowserView*>(browser->window());
    BrowserLauncherItemController* launcher_item_controller =
        browser_view->launcher_item_controller();
    if (!launcher_item_controller)
      return NULL;
    EXPECT_EQ(BrowserLauncherItemController::TYPE_EXTENSION_PANEL,
              launcher_item_controller->type());
    LauncherFaviconLoader* loader = launcher_item_controller->favicon_loader();
    return loader;
  }

  void ResetDownloads() {
    ASSERT_NE(static_cast<void*>(NULL), contents_observer_.get());
    contents_observer_->Reset();
  }

  bool WaitForFaviconDownlads(int expected) {
    const int64 max_seconds = 2;
    base::Time start_time = base::Time::Now();
    while (!contents_observer_->got_favicons() ||
           contents_observer_->downloads_received() < expected) {
      ui_test_utils::RunAllPendingInMessageLoop();
      base::TimeDelta delta = base::Time::Now() - start_time;
      if (delta.InSeconds() >= max_seconds) {
        LOG(ERROR) << " WaitForFaviconDownlads timed out:"
                   << " Got Favicons:" << contents_observer_->got_favicons()
                   << " Received: " << contents_observer_->downloads_received()
                   << " / " << expected;
        return false;
      }
    }
    return true;
  }

 private:
  scoped_ptr<ContentsObserver> contents_observer_;
  scoped_ptr<net::TestServer> test_server_;
};

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, SmallLauncherIcon) {
  ASSERT_TRUE(test_server()->Start());
  Browser* panel_browser;
  ASSERT_NO_FATAL_FAILURE(
      CreatePanelBrowser("launcher-smallfavicon.html", &panel_browser));
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader(panel_browser);
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);
  EXPECT_TRUE(WaitForFaviconDownlads(1));
  // No large favicons specified, bitmap should be empty.
  EXPECT_TRUE(favicon_loader->GetFavicon().empty());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, LargeLauncherIcon) {
  ASSERT_TRUE(test_server()->Start());
  Browser* panel_browser;
  ASSERT_NO_FATAL_FAILURE(
      CreatePanelBrowser("launcher-largefavicon.html", &panel_browser));
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader(panel_browser);
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);
  EXPECT_TRUE(WaitForFaviconDownlads(1));
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  EXPECT_EQ(128, favicon_loader->GetFavicon().height());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, ManyLauncherIcons) {
  ASSERT_TRUE(test_server()->Start());
  Browser* panel_browser;
  ASSERT_NO_FATAL_FAILURE(
      CreatePanelBrowser("launcher-manyfavicon.html", &panel_browser));
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader(panel_browser);
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);

  EXPECT_TRUE(WaitForFaviconDownlads(3));
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  // When multiple favicons are present, the correctly sized icon should be
  // chosen. The icons are sized assuming ash::kLauncherPreferredHeight < 128.
  EXPECT_GT(128, ash::kLauncherPreferredHeight);
  EXPECT_EQ(48, favicon_loader->GetFavicon().height());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, ChangeLauncherIcons) {
  ASSERT_TRUE(test_server()->Start());
  Browser* panel_browser;
  ASSERT_NO_FATAL_FAILURE(
      CreatePanelBrowser("launcher-manyfavicon.html", &panel_browser));
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader(panel_browser);
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);

  EXPECT_TRUE(WaitForFaviconDownlads(3));
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  EXPECT_EQ(48, favicon_loader->GetFavicon().height());
  ASSERT_NO_FATAL_FAILURE(ResetDownloads());

  NavigateTo(panel_browser, "launcher-smallfavicon.html");
  EXPECT_TRUE(WaitForFaviconDownlads(1));
  EXPECT_TRUE(favicon_loader->GetFavicon().empty());
  ASSERT_NO_FATAL_FAILURE(ResetDownloads());

  NavigateTo(panel_browser, "launcher-largefavicon.html");
  EXPECT_TRUE(WaitForFaviconDownlads(1));
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  EXPECT_EQ(128, favicon_loader->GetFavicon().height());
}
