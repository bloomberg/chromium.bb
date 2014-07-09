// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_favicon_loader.h"

#include "ash/shelf/shelf_constants.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::WebContents;
using content::WebContentsObserver;

namespace {

// Observer class to determine when favicons have completed loading.
class ContentsObserver : public WebContentsObserver {
 public:
  explicit ContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        loaded_(false),
        got_favicons_(false) {
  }

  virtual ~ContentsObserver() {}

  void Reset() {
    got_favicons_ = false;
  }

  bool loaded() const { return loaded_; }
  bool got_favicons() const { return got_favicons_; }

  // WebContentsObserver overrides.
  virtual void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                             const GURL& validated_url) OVERRIDE {
    loaded_ = true;
  }

  virtual void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) OVERRIDE {
    if (!candidates.empty())
      got_favicons_ = true;
  }

 private:
  bool loaded_;
  bool got_favicons_;
};

}  // namespace

class LauncherFaviconLoaderBrowsertest
    : public InProcessBrowserTest,
      public LauncherFaviconLoader::Delegate {
 public:
  LauncherFaviconLoaderBrowsertest() : favicon_updated_(false) {
  }

  virtual ~LauncherFaviconLoaderBrowsertest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    contents_observer_.reset(new ContentsObserver(web_contents));
    favicon_loader_.reset(new LauncherFaviconLoader(this, web_contents));
  }

  // LauncherFaviconLoader::Delegate overrides:
  virtual void FaviconUpdated() OVERRIDE {
    favicon_updated_ = true;
  }

 protected:
  bool NavigateTo(const char* url) {
    std::string url_path = base::StringPrintf("files/ash/launcher/%s", url);
    ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(url_path));
    return true;
  }

  bool WaitForContentsLoaded() {
    const int64 max_seconds = 10;
    base::Time start_time = base::Time::Now();
    while (!(contents_observer_->loaded() &&
             contents_observer_->got_favicons())) {
      content::RunAllPendingInMessageLoop();
      base::TimeDelta delta = base::Time::Now() - start_time;
      if (delta.InSeconds() >= max_seconds) {
        LOG(ERROR) << " WaitForContentsLoaded timed out.";
        return false;
      }
    }
    return true;
  }

  bool WaitForFaviconUpdated() {
    const int64 max_seconds = 10;
    base::Time start_time = base::Time::Now();
    while (favicon_loader_->HasPendingDownloads()) {
      content::RunAllPendingInMessageLoop();
      base::TimeDelta delta = base::Time::Now() - start_time;
      if (delta.InSeconds() >= max_seconds) {
        LOG(ERROR) << " WaitForFaviconDownlads timed out.";
        return false;
      }
    }
    return true;
  }

  void ResetDownloads() {
    contents_observer_->Reset();
  }

  bool favicon_updated_;
  scoped_ptr<ContentsObserver> contents_observer_;
  scoped_ptr<LauncherFaviconLoader> favicon_loader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherFaviconLoaderBrowsertest);
};

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, SmallLauncherIcon) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(NavigateTo("launcher-smallfavicon.html"));
  EXPECT_TRUE(WaitForContentsLoaded());
  EXPECT_TRUE(WaitForFaviconUpdated());

  // No large favicons specified, bitmap should be empty.
  EXPECT_TRUE(favicon_loader_->GetFavicon().empty());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, LargeLauncherIcon) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(NavigateTo("launcher-largefavicon.html"));
  EXPECT_TRUE(WaitForContentsLoaded());
  EXPECT_TRUE(WaitForFaviconUpdated());

  EXPECT_FALSE(favicon_loader_->GetFavicon().empty());
  EXPECT_EQ(128, favicon_loader_->GetFavicon().height());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, ManyLauncherIcons) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(NavigateTo("launcher-manyfavicon.html"));
  EXPECT_TRUE(WaitForContentsLoaded());
  EXPECT_TRUE(WaitForFaviconUpdated());

  EXPECT_FALSE(favicon_loader_->GetFavicon().empty());
  // When multiple favicons are present, the correctly sized icon should be
  // chosen. The icons are sized assuming ash::kShelfSize < 128.
  EXPECT_GT(128, ash::kShelfSize);
  EXPECT_EQ(48, favicon_loader_->GetFavicon().height());
}

IN_PROC_BROWSER_TEST_F(LauncherFaviconLoaderBrowsertest, ChangeLauncherIcons) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(NavigateTo("launcher-manyfavicon.html"));
  EXPECT_TRUE(WaitForContentsLoaded());
  EXPECT_TRUE(WaitForFaviconUpdated());

  EXPECT_FALSE(favicon_loader_->GetFavicon().empty());
  EXPECT_EQ(48, favicon_loader_->GetFavicon().height());
  ASSERT_NO_FATAL_FAILURE(ResetDownloads());

  ASSERT_TRUE(NavigateTo("launcher-smallfavicon.html"));
  ASSERT_TRUE(WaitForContentsLoaded());
  EXPECT_TRUE(WaitForFaviconUpdated());

  EXPECT_TRUE(favicon_loader_->GetFavicon().empty());
  ASSERT_NO_FATAL_FAILURE(ResetDownloads());

  ASSERT_TRUE(NavigateTo("launcher-largefavicon.html"));
  ASSERT_TRUE(WaitForContentsLoaded());
  EXPECT_TRUE(WaitForFaviconUpdated());

  EXPECT_FALSE(favicon_loader_->GetFavicon().empty());
  EXPECT_EQ(128, favicon_loader_->GetFavicon().height());
}
