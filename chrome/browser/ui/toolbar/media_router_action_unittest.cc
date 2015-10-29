// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"
#include "chrome/browser/ui/webui/media_router/media_router_test.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_utils.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_unittest_util.h"

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
  explicit TestMediaRouterAction(Browser* browser)
      : MediaRouterAction(browser),
        controller_(nullptr),
        platform_delegate_(nullptr) {}
  ~TestMediaRouterAction() override {}

  void SetMediaRouterDialogController(
      MediaRouterDialogControllerImpl* controller) {
    DCHECK(controller);
    controller_ = controller;
  }

 private:
  MediaRouterDialogControllerImpl* GetMediaRouterDialogController()
      override {
    return controller_;
  }

  MediaRouterActionPlatformDelegate* GetPlatformDelegate() override {
    return platform_delegate_;
  }

  MediaRouterDialogControllerImpl* controller_;
  MediaRouterActionPlatformDelegate* platform_delegate_;
};

class MediaRouterActionUnitTest : public MediaRouterTest {
 public:
  MediaRouterActionUnitTest()
      : fake_issue_notification_(media_router::Issue(
            "title notification",
            "message notification",
            media_router::IssueAction(media_router::IssueAction::TYPE_DISMISS),
            std::vector<media_router::IssueAction>(),
            "route_id",
            media_router::Issue::NOTIFICATION,
            false,
            std::string())),
        fake_issue_warning_(
            media_router::Issue("title warning",
                                "message warning",
                                media_router::IssueAction(
                                    media_router::IssueAction::TYPE_LEARN_MORE),
                                std::vector<media_router::IssueAction>(),
                                "route_id",
                                media_router::Issue::WARNING,
                                false,
                                "www.google.com")),
        fake_issue_fatal_(media_router::Issue(
            "title fatal",
            "message fatal",
            media_router::IssueAction(media_router::IssueAction::TYPE_DISMISS),
            std::vector<media_router::IssueAction>(),
            "route_id",
            media_router::Issue::FATAL,
            true,
            std::string())),
        fake_source1_("fakeSource1"),
        fake_source2_("fakeSource2"),
        active_icon_(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_MEDIA_ROUTER_ACTIVE_ICON)),
        error_icon_(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_MEDIA_ROUTER_ERROR_ICON)),
        idle_icon_(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_MEDIA_ROUTER_IDLE_ICON)),
        warning_icon_(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_MEDIA_ROUTER_WARNING_ICON)) {}

  ~MediaRouterActionUnitTest() override {}

  // MediaRouterTest:
  void SetUp() override {
    MediaRouterTest::SetUp();
    action_.reset(new TestMediaRouterAction(browser()));
  }

  void TearDown() override {
    action_.reset();
    MediaRouterTest::TearDown();
  }

  TestMediaRouterAction* action() { return action_.get(); }
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

 private:
  scoped_ptr<TestMediaRouterAction> action_;

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
  action()->OnHasLocalRouteUpdated(true);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      active_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since there are no local routes.
  action()->OnHasLocalRouteUpdated(false);
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
  action()->OnHasLocalRouteUpdated(false);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since there is a local route.
  action()->OnHasLocalRouteUpdated(true);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      active_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_|, with a priority to reflect the warning issue
  // rather than the local route.
  action()->OnIssueUpdated(fake_issue_warning());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      warning_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Closing a local route makes no difference to |current_icon_|.
  action()->OnHasLocalRouteUpdated(false);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      warning_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since the issue has been updated to fatal.
  action()->OnIssueUpdated(fake_issue_fatal());
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      error_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Fatal issues still take precedent over local routes.
  action()->OnHasLocalRouteUpdated(true);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      error_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // When the fatal issue is dismissed, |current_icon_| reflects the existing
  // local route.
  action()->OnIssueUpdated(nullptr);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      active_icon(), action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| when the local route is closed.
  action()->OnHasLocalRouteUpdated(false);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      idle_icon(), action()->GetIcon(nullptr, gfx::Size())));
}

TEST_F(MediaRouterActionUnitTest, IconPressedState) {
  // Start with one window with one tab.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator contents.
  WebContents* initiator_ =
      browser()->tab_strip_model()->GetActiveWebContents();

  MediaRouterDialogControllerImpl::CreateForWebContents(initiator_);
  MediaRouterDialogControllerImpl* dialog_controller_ =
      MediaRouterDialogControllerImpl::FromWebContents(initiator_);
  ASSERT_TRUE(dialog_controller_);

  // Sets the controller to use for TestMediaRouterAction.
  action()->SetMediaRouterDialogController(dialog_controller_);

  // Create a ToolbarActionViewDelegate to use for MediaRouterAction.
  scoped_ptr<MockToolbarActionViewDelegate> mock_delegate(
      new MockToolbarActionViewDelegate());

  EXPECT_CALL(*mock_delegate, GetCurrentWebContents()).WillOnce(
      testing::Return(initiator_));
  EXPECT_CALL(*mock_delegate, OnPopupClosed()).Times(1);
  action()->SetDelegate(mock_delegate.get());

  EXPECT_CALL(*mock_delegate, OnPopupShown(true)).Times(1);
  action()->ExecuteAction(true);

  EXPECT_CALL(*mock_delegate, OnPopupClosed()).Times(1);
  dialog_controller_->HideMediaRouterDialog();

  EXPECT_CALL(*mock_delegate, OnPopupShown(true)).Times(1);
  dialog_controller_->CreateMediaRouterDialog();

  EXPECT_CALL(*mock_delegate, OnPopupClosed()).Times(1);
  dialog_controller_->HideMediaRouterDialog();
}
