// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/media/router/media_router_ui_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr char kToolbarMigratedComponentActionStatus[] =
    "toolbar_migrated_component_action_status";
}

namespace media_router {

class MediaRouterUIBrowserTest : public InProcessBrowserTest {
 public:
  MediaRouterUIBrowserTest()
      : issue_(IssueInfo("title notification",
                         IssueInfo::Action::DISMISS,
                         IssueInfo::Severity::NOTIFICATION)) {}
  ~MediaRouterUIBrowserTest() override {}

  void SetUpOnMainThread() override {
    BrowserActionsContainer* browser_actions_container =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->browser_actions();
    ASSERT_TRUE(browser_actions_container);
    toolbar_actions_bar_ = browser_actions_container->toolbar_actions_bar();

    action_controller_ =
        MediaRouterUIService::Get(browser()->profile())->action_controller();

    routes_ = {MediaRoute("routeId1", MediaSource("sourceId"), "sinkId1",
                          "description", true, std::string(), true)};
  }

  void OpenMediaRouterDialogAndWaitForNewWebContents() {
    content::TestNavigationObserver nav_observer(NULL);
    nav_observer.StartWatchingNewWebContents();

    AppMenuButton* app_menu_button = GetAppMenuButton();

    // When the Media Router Action executes, it opens a dialog with web
    // contents to chrome://media-router.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaRouterUIBrowserTest::ExecuteMediaRouterAction,
                       base::Unretained(this), app_menu_button));

    base::RunLoop run_loop;
    app_menu_button->ShowMenu(false);
    run_loop.RunUntilIdle();

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

  ui::SimpleMenuModel* GetActionContextMenu() {
    return static_cast<ui::SimpleMenuModel*>(
        GetMediaRouterAction()->GetContextMenu());
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
    MediaRouterActionController::SetAlwaysShowActionPref(browser()->profile(),
                                                         always_show);
  }

  AppMenuButton* GetAppMenuButton() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->app_menu_button();
  }

  // Sets the old preference to show the toolbar action icon to |always_show|,
  // and migrates the preference.
  void MigrateToolbarIconPref(bool always_show) {
    {
      DictionaryPrefUpdate update(browser()->profile()->GetPrefs(),
                                  kToolbarMigratedComponentActionStatus);
      update->SetBoolean(ComponentToolbarActionsFactory::kMediaRouterActionId,
                         always_show);
    }
    MigrateObsoleteProfilePrefs(browser()->profile());
  }

 protected:
  ToolbarActionsBar* toolbar_actions_bar_ = nullptr;

  Issue issue_;

  // A vector of MediaRoutes that includes a local route.
  std::vector<MediaRoute> routes_;

  MediaRouterActionController* action_controller_ = nullptr;
};

#if defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_WIN)
// Flaky on chromeos, linux, win: https://crbug.com/658005
#define MAYBE_OpenDialogWithMediaRouterAction \
        DISABLED_OpenDialogWithMediaRouterAction
#else
#define MAYBE_OpenDialogWithMediaRouterAction OpenDialogWithMediaRouterAction
#endif

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       MAYBE_OpenDialogWithMediaRouterAction) {
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

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest, OpenDialogFromContextMenu) {
  // Start with one tab showing about:blank.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  MediaRouterDialogController* dialog_controller =
      MediaRouterDialogController::GetOrCreateForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  content::ContextMenuParams params;
  params.page_url = web_contents->GetController().GetActiveEntry()->GetURL();
  TestRenderViewContextMenu menu(web_contents->GetMainFrame(), params);
  menu.Init();

  ASSERT_TRUE(menu.IsItemPresent(IDC_ROUTE_MEDIA));
  ASSERT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  menu.ExecuteCommand(IDC_ROUTE_MEDIA, 0);
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  // Executing the command again should be a no-op, and there should only be one
  // dialog opened per tab.
  menu.ExecuteCommand(IDC_ROUTE_MEDIA, 0);
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());
  dialog_controller->HideMediaRouterDialog();
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest, OpenDialogFromAppMenu) {
  // Start with one tab showing about:blank.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  AppMenuButton* menu_button = GetAppMenuButton();
  base::RunLoop run_loop;
  menu_button->ShowMenu(false);
  run_loop.RunUntilIdle();

  MediaRouterDialogController* dialog_controller =
      MediaRouterDialogController::GetOrCreateForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  AppMenu* menu = menu_button->app_menu_for_testing();
  ASSERT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  menu->ExecuteCommand(IDC_ROUTE_MEDIA, 0);
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  // Executing the command again should be a no-op, and there should only be one
  // dialog opened per tab.
  menu->ExecuteCommand(IDC_ROUTE_MEDIA, 0);
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());
  dialog_controller->HideMediaRouterDialog();
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest, OpenDialogsInMultipleTabs) {
  // Start with two tabs.
  chrome::NewTab(browser());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  MediaRouterDialogController* dialog_controller1 =
      MediaRouterDialogController::GetOrCreateForWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(0));
  MediaRouterDialogController* dialog_controller2 =
      MediaRouterDialogController::GetOrCreateForWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(1));

  // Show the media router action on the toolbar.
  SetAlwaysShowActionPref(true);

  // Open a dialog in the first tab using the toolbar action.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_FALSE(dialog_controller1->IsShowingMediaRouterDialog());
  GetMediaRouterAction()->ExecuteAction(true);
  EXPECT_TRUE(dialog_controller1->IsShowingMediaRouterDialog());

  // Move to the second tab, which shouldn't have a dialog at first. Open and
  // close a dialog in that tab.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_FALSE(dialog_controller2->IsShowingMediaRouterDialog());
  GetMediaRouterAction()->ExecuteAction(true);
  EXPECT_TRUE(dialog_controller2->IsShowingMediaRouterDialog());
  GetMediaRouterAction()->ExecuteAction(true);
  EXPECT_FALSE(dialog_controller2->IsShowingMediaRouterDialog());

  // Move back to the first tab, whose dialog should still be open. Hide the
  // dialog.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_TRUE(dialog_controller1->IsShowingMediaRouterDialog());
  GetMediaRouterAction()->ExecuteAction(true);
  EXPECT_FALSE(dialog_controller1->IsShowingMediaRouterDialog());

  // Reset the preference showing the toolbar action.
  SetAlwaysShowActionPref(false);
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       EphemeralToolbarIconForRoutesAndIssues) {
  action_controller_->OnIssue(issue_);
  EXPECT_TRUE(ActionExists());
  action_controller_->OnIssuesCleared();
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

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest, UpdateActionLocation) {
  SetAlwaysShowActionPref(true);

  // Get the index for "Hide in Chrome menu" / "Show in toolbar" menu item.
  const int command_index = GetActionContextMenu()->GetIndexOfCommandId(
      IDC_MEDIA_ROUTER_SHOW_IN_TOOLBAR);

  // Start out with the action visible on the main bar.
  EXPECT_TRUE(
      toolbar_actions_bar_->IsActionVisibleOnMainBar(GetMediaRouterAction()));
  GetActionContextMenu()->ActivatedAt(command_index);

  // The action should get hidden in the overflow menu.
  EXPECT_FALSE(
      toolbar_actions_bar_->IsActionVisibleOnMainBar(GetMediaRouterAction()));
  GetActionContextMenu()->ActivatedAt(command_index);

  // The action should be back on the main bar.
  EXPECT_TRUE(
      toolbar_actions_bar_->IsActionVisibleOnMainBar(GetMediaRouterAction()));
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest, MigrateToolbarIconShownPref) {
  MigrateToolbarIconPref(true);
  EXPECT_TRUE(MediaRouterActionController::GetAlwaysShowActionPref(
      browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       MigrateToolbarIconUnshownPref) {
  MigrateToolbarIconPref(false);
  EXPECT_FALSE(MediaRouterActionController::GetAlwaysShowActionPref(
      browser()->profile()));
}

}  // namespace media_router
