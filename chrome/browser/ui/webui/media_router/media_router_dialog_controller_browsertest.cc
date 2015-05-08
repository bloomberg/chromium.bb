// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

using content::WebContents;
using content::TestNavigationObserver;

namespace media_router {

class MediaRouterDialogControllerBrowserTest : public InProcessBrowserTest {
 public:
  MediaRouterDialogControllerBrowserTest()
      : dialog_controller_(nullptr),
        initiator_(nullptr),
        media_router_dialog_(nullptr) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableMediaRouter);
  }
  ~MediaRouterDialogControllerBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    // Start with one window with one tab.
    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
    EXPECT_EQ(1, browser()->tab_strip_model()->count());

    initiator_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(initiator_);
    MediaRouterDialogController::CreateForWebContents(initiator_);
    dialog_controller_ =
        MediaRouterDialogController::FromWebContents(initiator_);
    ASSERT_TRUE(dialog_controller_);

    // Get the media router dialog for the initiator.
    media_router_dialog_ = dialog_controller_->ShowMediaRouterDialog();
    ASSERT_TRUE(media_router_dialog_);
  }

  MediaRouterDialogController* dialog_controller_;
  WebContents* initiator_;
  WebContents* media_router_dialog_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MediaRouterDialogControllerBrowserTest, ShowDialog) {
  // Waits for the dialog to initialize.
  TestNavigationObserver nav_observer(media_router_dialog_);
  nav_observer.Wait();

  // New media router dialog is a constrained window, so the number of
  // tabs is still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(initiator_, media_router_dialog_);
  EXPECT_EQ(media_router_dialog_, dialog_controller_->GetMediaRouterDialog());

  content::WebUI* web_ui = media_router_dialog_->GetWebUI();
  ASSERT_TRUE(web_ui);
  MediaRouterUI* media_router_ui =
      static_cast<MediaRouterUI*>(web_ui->GetController());
  ASSERT_TRUE(media_router_ui);
}

IN_PROC_BROWSER_TEST_F(MediaRouterDialogControllerBrowserTest, Navigate) {
  // New media router dialog is a constrained window, so the number of
  // tabs is still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(media_router_dialog_, dialog_controller_->GetMediaRouterDialog());

  // Navigate to another URL.
  content::WebContentsDestroyedWatcher dialog_watcher(media_router_dialog_);

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // Blocks until the dialog WebContents has been destroyed.
  dialog_watcher.Wait();

  // Entry has been removed.
  EXPECT_FALSE(dialog_controller_->GetMediaRouterDialog());
}

IN_PROC_BROWSER_TEST_F(MediaRouterDialogControllerBrowserTest,
    RenderProcessHost) {
  // New media router dialog is a constrained window, so the number of
  // tabs is still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(media_router_dialog_, dialog_controller_->GetMediaRouterDialog());

  // Crash initiator_'s renderer process.
  content::WebContentsDestroyedWatcher dialog_watcher(media_router_dialog_);
  content::RenderProcessHostWatcher rph_watcher(initiator_,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  ui_test_utils::NavigateToURL(browser(), GURL(content::kChromeUICrashURL));

  // Blocks until the dialog WebContents has been destroyed.
  rph_watcher.Wait();
  dialog_watcher.Wait();

  // Entry has been removed.
  EXPECT_FALSE(dialog_controller_->GetMediaRouterDialog());
}

}  // namespace media_router
