// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"
#include "chrome/browser/ui/webui/media_router/media_router_test.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"

using content::WebContents;
using media_router::MediaRouterDialogControllerImpl;

class MockToolbarActionViewDelegate : public ToolbarActionViewDelegate {
 public:
  MockToolbarActionViewDelegate() {}
  ~MockToolbarActionViewDelegate() {}

  MOCK_CONST_METHOD0(GetCurrentWebContents, WebContents*());
  MOCK_METHOD0(UpdateState, void());
  MOCK_CONST_METHOD0(IsMenuRunning, bool());
  MOCK_METHOD1(OnPopupShown, void(bool by_user));
  MOCK_METHOD0(OnPopupClosed, void());
};

class TestMediaRouterAction : public MediaRouterAction {
 public:
  TestMediaRouterAction(Browser* browser,
                        ToolbarActionsBar* toolbar_actions_bar)
      : MediaRouterAction(browser, toolbar_actions_bar),
        controller_(nullptr),
        platform_delegate_(nullptr) {}
  ~TestMediaRouterAction() override {}

  void SetMediaRouterDialogController(
      MediaRouterDialogControllerImpl* controller) {
    DCHECK(controller);
    controller_ = controller;
    controller_->SetMediaRouterAction(GetWeakPtr());
  }

 private:
  MediaRouterDialogControllerImpl* GetMediaRouterDialogController()
      override {
    return controller_;
  }

  MediaRouterActionPlatformDelegate* GetPlatformDelegate() override {
    return platform_delegate_;
  }

  void MaybeRemoveAction() override {
    if (GetMediaRouterDialogController())
      MediaRouterAction::MaybeRemoveAction();
  }

  MediaRouterDialogControllerImpl* controller_;
  MediaRouterActionPlatformDelegate* platform_delegate_;
};

class MediaRouterActionUnitTest : public MediaRouterTest {
 public:
  MediaRouterActionUnitTest()
      : toolbar_model_(nullptr),
        fake_issue_notification_(media_router::Issue(
            "title notification",
            "message notification",
            media_router::IssueAction(media_router::IssueAction::TYPE_DISMISS),
            std::vector<media_router::IssueAction>(),
            "route_id",
            media_router::Issue::NOTIFICATION,
            false,
            -1)),
        fake_issue_warning_(
            media_router::Issue("title warning",
                                "message warning",
                                media_router::IssueAction(
                                    media_router::IssueAction::TYPE_LEARN_MORE),
                                std::vector<media_router::IssueAction>(),
                                "route_id",
                                media_router::Issue::WARNING,
                                false,
                                12345)),
        fake_issue_fatal_(media_router::Issue(
            "title fatal",
            "message fatal",
            media_router::IssueAction(media_router::IssueAction::TYPE_DISMISS),
            std::vector<media_router::IssueAction>(),
            "route_id",
            media_router::Issue::FATAL,
            true,
            -1)),
        fake_source1_("fakeSource1"),
        fake_source2_("fakeSource2"),
        active_icon_(
            gfx::CreateVectorIcon(gfx::VectorIconId::MEDIA_ROUTER_ACTIVE,
                                  gfx::kPlaceholderColor)),
        error_icon_(gfx::CreateVectorIcon(gfx::VectorIconId::MEDIA_ROUTER_ERROR,
                                          gfx::kPlaceholderColor)),
        idle_icon_(gfx::CreateVectorIcon(gfx::VectorIconId::MEDIA_ROUTER_IDLE,
                                         gfx::kPlaceholderColor)),
        warning_icon_(
            gfx::CreateVectorIcon(gfx::VectorIconId::MEDIA_ROUTER_WARNING,
                                  gfx::kPlaceholderColor)) {}

  ~MediaRouterActionUnitTest() override {}

  // MediaRouterTest:
  void SetUp() override {
    MediaRouterTest::SetUp();
    static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile()))
        ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                 base::FilePath(), false);
    toolbar_model_ = extensions::extension_action_test_util::
        CreateToolbarModelForProfile(profile());

    // browser() will only be valid once BrowserWithTestWindowTest::SetUp()
    // has run.
    browser_action_test_util_.reset(
        new BrowserActionTestUtil(browser(), false));
    action_.reset(
        new TestMediaRouterAction(
            browser(),
            browser_action_test_util_->GetToolbarActionsBar()));
    delegate_.reset(new MockToolbarActionViewDelegate());

    action()->SetDelegate(delegate_.get());

    local_display_route_list_.push_back(
        media_router::MediaRoute("routeId1", fake_source1_, "sinkId1",
                                 "description", true, std::string(), true));
    non_local_display_route_list_.push_back(
        media_router::MediaRoute("routeId2", fake_source1_, "sinkId2",
                                 "description", false, std::string(), true));
    non_local_display_route_list_.push_back(
        media_router::MediaRoute("routeId3", fake_source2_, "sinkId3",
                                 "description", true, std::string(), false));
  }

  void TearDown() override {
    delegate_.reset();
    action_.reset();
    browser_action_test_util_.reset();
    MediaRouterTest::TearDown();
  }

  bool ActionExists() {
    return toolbar_model_->HasComponentAction(
        ComponentToolbarActionsFactory::kMediaRouterActionId);
  }

  void ResetTestMediaRouterAction() {
    action_.reset();
  }

  TestMediaRouterAction* action() { return action_.get(); }
  ToolbarActionsModel* toolbar_model() { return toolbar_model_; }
  const media_router::Issue* fake_issue_notification() {
    return &fake_issue_notification_;
  }
  const media_router::Issue* fake_issue_warning() {
    return &fake_issue_warning_;
  }
  const media_router::Issue* fake_issue_fatal() {
    return &fake_issue_fatal_;
  }
  const gfx::Image active_icon() { return active_icon_; }
  const gfx::Image error_icon() { return error_icon_; }
  const gfx::Image idle_icon() { return idle_icon_; }
  const gfx::Image warning_icon() { return warning_icon_; }
  const std::vector<media_router::MediaRoute>& local_display_route_list()
      const {
    return local_display_route_list_;
  }
  const std::vector<media_router::MediaRoute>& non_local_display_route_list()
      const {
    return non_local_display_route_list_;
  }
  const std::vector<media_router::MediaRoute::Id>& empty_route_id_list() const {
    return empty_route_id_list_;
  }

 private:
  // A BrowserActionTestUtil object constructed with the associated
  // ToolbarActionsBar.
  std::unique_ptr<BrowserActionTestUtil> browser_action_test_util_;

  std::unique_ptr<TestMediaRouterAction> action_;
  std::unique_ptr<MockToolbarActionViewDelegate> delegate_;

  // The associated ToolbarActionsModel (owned by the keyed service setup).
  ToolbarActionsModel* toolbar_model_;

  // Fake Issues.
  const media_router::Issue fake_issue_notification_;
  const media_router::Issue fake_issue_warning_;
  const media_router::Issue fake_issue_fatal_;

  // Fake Sources, used for the Routes.
  const media_router::MediaSource fake_source1_;
  const media_router::MediaSource fake_source2_;

  // Cached images.
  const gfx::Image active_icon_;
  const gfx::Image error_icon_;
  const gfx::Image idle_icon_;
  const gfx::Image warning_icon_;

  std::vector<media_router::MediaRoute> local_display_route_list_;
  std::vector<media_router::MediaRoute> non_local_display_route_list_;
  std::vector<media_router::MediaRoute::Id> empty_route_id_list_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterActionUnitTest);
};

// Tests the initial state of MediaRouterAction after construction.
TEST_F(MediaRouterActionUnitTest, Initialization) {
  EXPECT_EQ("media_router_action", action()->GetId());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TITLE),
      action()->GetActionName());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));
}

// Tests the MediaRouterAction icon based on updates to issues.
TEST_F(MediaRouterActionUnitTest, UpdateIssues) {
  // Initially, there are no issues.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Don't update |current_icon_| since the issue is only a notification.
  action()->OnIssueUpdated(fake_issue_notification());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since the issue is a warning.
  action()->OnIssueUpdated(fake_issue_warning());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      warning_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since the issue is fatal.
  action()->OnIssueUpdated(fake_issue_fatal());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      error_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Clear the issue.
  action()->OnIssueUpdated(nullptr);
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));
}

// Tests the MediaRouterAction state updates based on whether there are local
// routes.
TEST_F(MediaRouterActionUnitTest, UpdateRoutes) {
  // Initially, there are no routes.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since there is a local route.
  action()->OnRoutesUpdated(local_display_route_list(), empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      active_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since there are no local routes.
  action()->OnRoutesUpdated(non_local_display_route_list(),
                            empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));

  action()->OnRoutesUpdated(std::vector<media_router::MediaRoute>(),
                            empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));
}

// Tests the MediaRouterAction icon based on updates to both issues and routes.
TEST_F(MediaRouterActionUnitTest, UpdateIssuesAndRoutes) {
  // Initially, there are no issues or routes.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // There is no change in |current_icon_| since notification issues do not
  // update the state.
  action()->OnIssueUpdated(fake_issue_notification());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Non-local routes also do not have an effect on |current_icon_|.
  action()->OnRoutesUpdated(non_local_display_route_list(),
                            empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since there is a local route.
  action()->OnRoutesUpdated(local_display_route_list(), empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      active_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_|, with a priority to reflect the warning issue
  // rather than the local route.
  action()->OnIssueUpdated(fake_issue_warning());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      warning_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Closing a local route makes no difference to |current_icon_|.
  action()->OnRoutesUpdated(non_local_display_route_list(),
                            empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      warning_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since the issue has been updated to fatal.
  action()->OnIssueUpdated(fake_issue_fatal());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      error_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Fatal issues still take precedent over local routes.
  action()->OnRoutesUpdated(local_display_route_list(), empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      error_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // When the fatal issue is dismissed, |current_icon_| reflects the existing
  // local route.
  action()->OnIssueUpdated(nullptr);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      active_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| when the local route is closed.
  action()->OnRoutesUpdated(non_local_display_route_list(),
                            empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));
}

TEST_F(MediaRouterActionUnitTest, IconPressedState) {
  base::AutoReset<bool> disable_animations(
      &ToolbarActionsBar::disable_animations_for_testing_, true);

  // Start with one window with one tab.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();
  MediaRouterDialogControllerImpl::CreateForWebContents(initiator);
  MediaRouterDialogControllerImpl* dialog_controller =
      MediaRouterDialogControllerImpl::FromWebContents(initiator);
  ASSERT_TRUE(dialog_controller);

  // Sets the controller to use for TestMediaRouterAction.
  action()->SetMediaRouterDialogController(dialog_controller);

  // Add the icon to the toolbar and make it persist.
  toolbar_model()->AddComponentAction(
      ComponentToolbarActionsFactory::kMediaRouterActionId);
  action()->ToggleVisibilityPreference();
  EXPECT_TRUE(ActionExists());

  action()->ExecuteAction(true);
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  // Pressing the icon while the popup is shown should close the popup
  action()->ExecuteAction(true);
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());

  dialog_controller->CreateMediaRouterDialog();
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  dialog_controller->HideMediaRouterDialog();
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());

  // Make the icon go away.
  action()->ToggleVisibilityPreference();
  EXPECT_FALSE(ActionExists());
}

TEST_F(MediaRouterActionUnitTest, EphemeralIcon) {
  // We'll be using the action created by the toolbar model in this test,
  // so we remove the test action here.
  ResetTestMediaRouterAction();

  base::AutoReset<bool> disable_animations(
      &ToolbarActionsBar::disable_animations_for_testing_, true);

  // Start with one window with one tab.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();
  MediaRouterDialogControllerImpl::CreateForWebContents(initiator);
  MediaRouterDialogControllerImpl* dialog_controller =
      MediaRouterDialogControllerImpl::FromWebContents(initiator);
  ASSERT_TRUE(dialog_controller);

  EXPECT_FALSE(ActionExists());
  // Show the popup. The icon should become visible.
  dialog_controller->ShowMediaRouterDialog();
  EXPECT_TRUE(ActionExists());
  // Hide the popup. The icon should become hidden.
  dialog_controller->HideMediaRouterDialog();
  EXPECT_FALSE(ActionExists());

  // Show the popup.
  dialog_controller->ShowMediaRouterDialog();
  // Add a local display route.
  dialog_controller->action_for_test()
      ->OnRoutesUpdated(local_display_route_list(), empty_route_id_list());
  EXPECT_TRUE(ActionExists());
  // Hide the popup while there's still a local media route. The icon should not
  // be hidden as long as the media route exists.
  dialog_controller->HideMediaRouterDialog();
  EXPECT_TRUE(ActionExists());
  // Remove the local route. Now the icon should be hidden.
  dialog_controller->action_for_test()->OnRoutesUpdated(
      std::vector<media_router::MediaRoute>(),
      empty_route_id_list());
  EXPECT_FALSE(ActionExists());
}

TEST_F(MediaRouterActionUnitTest, ToggleIconVisibilityPreference) {
  // We'll be using the action created by the toolbar model in this test,
  // so we remove the test action here.
  ResetTestMediaRouterAction();

  base::AutoReset<bool> disable_animations(
      &ToolbarActionsBar::disable_animations_for_testing_, true);

  // Start with one window with one tab.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();
  MediaRouterDialogControllerImpl::CreateForWebContents(initiator);
  MediaRouterDialogControllerImpl* dialog_controller =
      MediaRouterDialogControllerImpl::FromWebContents(initiator);
  ASSERT_TRUE(dialog_controller);

  EXPECT_FALSE(ActionExists());
  // Show the popup. The icon should become visible.
  dialog_controller->ShowMediaRouterDialog();
  EXPECT_TRUE(ActionExists());
  // Turn on the settings to always show the icon.
  dialog_controller->action_for_test()->ToggleVisibilityPreference();
  // Hide the popup. The icon should stay visible.
  dialog_controller->HideMediaRouterDialog();
  EXPECT_TRUE(ActionExists());

  // Close the tab and open another.
  browser()->tab_strip_model()->CloseAllTabs();
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  // The icon should persist.
  EXPECT_TRUE(ActionExists());
  // Get the dialog controller for the new tab.
  dialog_controller = MediaRouterDialogControllerImpl::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Turn off the setting to always show the icon.
  // The icon should get hidden now.
  dialog_controller->action_for_test()->ToggleVisibilityPreference();
  EXPECT_FALSE(ActionExists());
}
