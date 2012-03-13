// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_path.h"
#include "base/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/ash/launcher/launcher_favicon_loader.h"
#include "chrome/browser/ui/views/ash/launcher/launcher_updater.h"
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

  Browser* CreatePanelBrowser(const char* url) {
    Browser::CreateParams params(Browser::TYPE_PANEL, browser()->profile());
    params.app_name = "Test Panel";
    Browser* browser =  Browser::CreateWithParams(params);
    EXPECT_TRUE(browser->is_type_panel());
    CHECK(!contents_observer_.get());
    // Load initial tab contents before setting the observer.
    ui_test_utils::NavigateToURL(browser, GURL());
    contents_observer_.reset(
        new ContentsObserver(browser->GetWebContentsAt(0)));
    NavigateTo(browser, url);
    return browser;
  }

  LauncherFaviconLoader* GetFaviconLoader(Browser* browser) {
    BrowserView* browser_view = static_cast<BrowserView*>(browser->window());
    LauncherUpdater* launcher_updater = browser_view->icon_updater();
    if (!launcher_updater)
      return NULL;
    EXPECT_EQ(LauncherUpdater::TYPE_PANEL, launcher_updater->type());
    LauncherFaviconLoader* loader = launcher_updater->favicon_loader();
    return loader;
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
  Browser* panel_browser = CreatePanelBrowser("launcher-smallfavicon.html");
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader(panel_browser);
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);
  EXPECT_TRUE(WaitForFaviconDownlads(1));
  // No large favicons specified, bitmap should be empty.
  EXPECT_TRUE(favicon_loader->GetFavicon().empty());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, LargeLauncherIcon) {
  ASSERT_TRUE(test_server()->Start());
  Browser* panel_browser = CreatePanelBrowser("launcher-largefavicon.html");
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader(panel_browser);
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);
  EXPECT_TRUE(WaitForFaviconDownlads(1));
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  EXPECT_EQ(128, favicon_loader->GetFavicon().height());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, ManyLauncherIcons) {
  ASSERT_TRUE(test_server()->Start());
  Browser* panel_browser = CreatePanelBrowser("launcher-manyfavicon.html");
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader(panel_browser);
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  // When multiple favicons are present, the correctly sized icon should be
  // chosen. The icons are sized assuming ash::kLauncherPreferredHeight < 128.
  EXPECT_TRUE(WaitForFaviconDownlads(3));
  EXPECT_GT(128, ash::kLauncherPreferredHeight);
  EXPECT_EQ(48, favicon_loader->GetFavicon().height());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, ChangeLauncherIcons) {
  ASSERT_TRUE(test_server()->Start());
  Browser* panel_browser = CreatePanelBrowser("launcher-manyfavicon.html");
  LauncherFaviconLoader* favicon_loader = GetFaviconLoader(panel_browser);
  ASSERT_NE(static_cast<LauncherFaviconLoader*>(NULL), favicon_loader);
  WaitForFaviconDownlads(3);
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  EXPECT_EQ(48, favicon_loader->GetFavicon().height());
  NavigateTo(panel_browser, "launcher-smallfavicon.html");
  EXPECT_TRUE(WaitForFaviconDownlads(1));
  EXPECT_TRUE(favicon_loader->GetFavicon().empty());
  NavigateTo(panel_browser, "launcher-largefavicon.html");
  EXPECT_TRUE(WaitForFaviconDownlads(1));
  EXPECT_FALSE(favicon_loader->GetFavicon().empty());
  EXPECT_EQ(128, favicon_loader->GetFavicon().height());
}
