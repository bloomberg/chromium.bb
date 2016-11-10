// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/media/router/media_router_ui_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/views/widget/widget.h"

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
    toolbar_actions_bar_ = browser_actions_container->toolbar_actions_bar();

    action_controller_ =
        MediaRouterUIService::Get(browser()->profile())->action_controller();

    issue_.reset(new Issue(
        "title notification", "message notification",
        media_router::IssueAction(media_router::IssueAction::TYPE_DISMISS),
        std::vector<media_router::IssueAction>(), "route_id",
        media_router::Issue::NOTIFICATION, false, -1));

    routes_ = {MediaRoute("routeId1", MediaSource("sourceId"), "sinkId1",
                          "description", true, std::string(), true)};
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
        base::Bind(&MediaRouterUIBrowserTest::ExecuteMediaRouterAction,
                   base::Unretained(this), app_menu_button));
    app_menu_button->ShowMenu(false);

    nav_observer.Wait();
    EXPECT_FALSE(app_menu_button->IsMenuShowing());
    ASSERT_EQ(chrome::kChromeUIMediaRouterURL,
        nav_observer.last_navigation_url().spec());
    nav_observer.StopWatchingNewWebContents();
  }

  MediaRouterAction* GetMediaRouterAction() {
    return MediaRouterDialogControllerImpl::GetOrCreateForWebContents(
               browser()->tab_strip_model()->GetActiveWebContents())
        ->action();
  }

  void ExecuteMediaRouterAction(AppMenuButton* app_menu_button) {
    EXPECT_TRUE(app_menu_button->IsMenuShowing());
    GetMediaRouterAction()->ExecuteAction(true);
  }

  bool ActionExists() {
    return ToolbarActionsModel::Get(browser()->profile())
        ->HasComponentAction(
            ComponentToolbarActionsFactory::kMediaRouterActionId);
  }

  void SetAlwaysShowActionPref(bool always_show) {
    return ToolbarActionsModel::Get(browser()->profile())
        ->component_migration_helper()
        ->SetComponentActionPref(
            ComponentToolbarActionsFactory::kMediaRouterActionId, always_show);
  }

 protected:
  ToolbarActionsBar* toolbar_actions_bar_ = nullptr;

  std::unique_ptr<Issue> issue_;

  // A vector of MediaRoutes that includes a local route.
  std::vector<MediaRoute> routes_;

  MediaRouterActionController* action_controller_ = nullptr;
};

// TODO(crbug.com/658005): Fails on multiple platforms.
IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       DISABLED_OpenDialogWithMediaRouterAction) {
  // We start off at about:blank page.
  // Make sure there is 1 tab and media router is enabled.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  SetAlwaysShowActionPref(true);
  EXPECT_TRUE(ActionExists());

  OpenMediaRouterDialogAndWaitForNewWebContents();

  // Reload the browser and wait.
  content::TestNavigationObserver reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  reload_observer.Wait();

  // The reload should have removed the previously created dialog.
  // We expect a new dialog WebContents to be created by calling this.
  OpenMediaRouterDialogAndWaitForNewWebContents();

  // Navigate away.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // The navigation should have removed the previously created dialog.
  // We expect a new dialog WebContents to be created by calling this.
  OpenMediaRouterDialogAndWaitForNewWebContents();

  SetAlwaysShowActionPref(false);
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       EphemeralToolbarIconForRoutesAndIssues) {
  action_controller_->OnIssueUpdated(issue_.get());
  EXPECT_TRUE(ActionExists());
  action_controller_->OnIssueUpdated(nullptr);
  EXPECT_FALSE(ActionExists());

  action_controller_->OnRoutesUpdated(routes_, std::vector<MediaRoute::Id>());
  EXPECT_TRUE(ActionExists());
  action_controller_->OnRoutesUpdated(std::vector<MediaRoute>(),
                                      std::vector<MediaRoute::Id>());
  EXPECT_FALSE(ActionExists());

  SetAlwaysShowActionPref(true);
  EXPECT_TRUE(ActionExists());
  SetAlwaysShowActionPref(false);
  EXPECT_FALSE(ActionExists());
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       EphemeralToolbarIconForDialog) {
  MediaRouterDialogController* dialog_controller =
      MediaRouterDialogController::GetOrCreateForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_FALSE(ActionExists());
  dialog_controller->ShowMediaRouterDialog();
  EXPECT_TRUE(ActionExists());
  dialog_controller->HideMediaRouterDialog();
  EXPECT_FALSE(ActionExists());

  dialog_controller->ShowMediaRouterDialog();
  EXPECT_TRUE(ActionExists());
  // Clicking on the toolbar icon should hide both the dialog and the icon.
  GetMediaRouterAction()->ExecuteAction(true);
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  EXPECT_FALSE(ActionExists());

  dialog_controller->ShowMediaRouterDialog();
  SetAlwaysShowActionPref(true);
  // When the pref is set to true, hiding the dialog shouldn't hide the icon.
  dialog_controller->HideMediaRouterDialog();
  EXPECT_TRUE(ActionExists());
  dialog_controller->ShowMediaRouterDialog();
  // While the dialog is showing, setting the pref to false shouldn't hide the
  // icon.
  SetAlwaysShowActionPref(false);
  EXPECT_TRUE(ActionExists());
  dialog_controller->HideMediaRouterDialog();
  EXPECT_FALSE(ActionExists());
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       EphemeralToolbarIconWithMultipleWindows) {
  action_controller_->OnRoutesUpdated(routes_, std::vector<MediaRoute::Id>());
  EXPECT_TRUE(ActionExists());

  // Opening and closing a window shouldn't affect the state of the ephemeral
  // icon. Creating and removing the icon with multiple windows open should also
  // work.
  Browser* browser2 = CreateBrowser(browser()->profile());
  EXPECT_TRUE(ActionExists());
  action_controller_->OnRoutesUpdated(std::vector<MediaRoute>(),
                                      std::vector<MediaRoute::Id>());
  EXPECT_FALSE(ActionExists());
  action_controller_->OnRoutesUpdated(routes_, std::vector<MediaRoute::Id>());
  EXPECT_TRUE(ActionExists());
  browser2->window()->Close();
  EXPECT_TRUE(ActionExists());
}

}  // namespace media_router
