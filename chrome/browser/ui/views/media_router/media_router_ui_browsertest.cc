// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"

namespace media_router {

class MediaRouterUIBrowserTest : public InProcessBrowserTest {
 public:
  MediaRouterUIBrowserTest() {}
  ~MediaRouterUIBrowserTest() override {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    BrowserActionsContainer* browser_actions_container =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->browser_actions();
    ASSERT_TRUE(browser_actions_container);

    media_router_action_.reset(new MediaRouterAction(browser()));

    // Sets delegate on |media_router_action_|.
    toolbar_action_view_.reset(
        new ToolbarActionView(media_router_action_.get(), browser()->profile(),
                              browser_actions_container));
  }

  void TearDownOnMainThread() override {
    toolbar_action_view_.reset();
    media_router_action_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void OpenMediaRouterDialogAndWaitForNewWebContents() {
    content::TestNavigationObserver nav_observer(NULL);
    nav_observer.StartWatchingNewWebContents();

    AppMenuButton* app_menu_button =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->app_menu_button();

    // When the Media Router Action executes, it opens a dialog with web
    // contents to chrome://media-router.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&MediaRouterUIBrowserTest::ExecuteMediaRouterAction, this,
                   app_menu_button));
    app_menu_button->ShowMenu(false);

    EXPECT_FALSE(app_menu_button->IsMenuShowing());
    nav_observer.Wait();
    ASSERT_EQ(chrome::kChromeUIMediaRouterURL,
        nav_observer.last_navigation_url().spec());
    nav_observer.StopWatchingNewWebContents();
  }

  void ExecuteMediaRouterAction(AppMenuButton* app_menu_button) {
    EXPECT_TRUE(app_menu_button->IsMenuShowing());
    media_router_action_->ExecuteAction(true);
  }

 protected:
  // Must be initialized after |InProcessBrowserTest::SetUpOnMainThread|.
  scoped_ptr<MediaRouterAction> media_router_action_;

  // ToolbarActionView constructed to set the delegate on |mr_action|.
  scoped_ptr<ToolbarActionView> toolbar_action_view_;
};

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       OpenDialogWithMediaRouterAction) {
  // We start off at about:blank page.
  // Make sure there is 1 tab and media router is enabled.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  OpenMediaRouterDialogAndWaitForNewWebContents();

  // Reload the browser and wait.
  content::TestNavigationObserver reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::Reload(browser(), CURRENT_TAB);
  reload_observer.Wait();

  // The reload should have removed the previously created dialog.
  // We expect a new dialog WebContents to be created by calling this.
  OpenMediaRouterDialogAndWaitForNewWebContents();

  // Navigate away.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // The navigation should have removed the previously created dialog.
  // We expect a new dialog WebContents to be created by calling this.
  OpenMediaRouterDialogAndWaitForNewWebContents();
}

}  // namespace media_router
