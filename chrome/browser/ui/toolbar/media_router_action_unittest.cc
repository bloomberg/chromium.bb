// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/webui/media_router/media_router_test.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/test_utils.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_unittest_util.h"

class MediaRouterActionUnitTest : public MediaRouterTest {
 public:
  MediaRouterActionUnitTest()
      : fake_issue_notification_(media_router::Issue(
            "title notification",
            "message notification",
            media_router::IssueAction(media_router::IssueAction::TYPE_OK),
            std::vector<media_router::IssueAction>(),
            "route_id",
            media_router::Issue::NOTIFICATION,
            false, std::string())),
        fake_issue_warning_(media_router::Issue(
            "title warning",
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
            media_router::IssueAction(media_router::IssueAction::TYPE_OK),
            std::vector<media_router::IssueAction>(),
            "route_id",
            media_router::Issue::FATAL,
            true,
            std::string())),
        fake_sink1_("fakeSink1", "Fake Sink 1"),
        fake_sink2_("fakeSink2", "Fake Sink 2"),
        fake_source1_("fakeSource1"),
        fake_source2_("fakeSource2"),
        fake_route_local_("route1", fake_source1_, fake_sink1_, "desc1",
            true, "path.html"),
        fake_route_remote_("route2", fake_source2_, fake_sink2_, "desc2",
            false, "path.html"),
        active_icon_(ui::ResourceBundle::GetSharedInstance().
            GetImageNamed(IDR_MEDIA_ROUTER_ACTIVE_ICON)),
        error_icon_(ui::ResourceBundle::GetSharedInstance().
            GetImageNamed(IDR_MEDIA_ROUTER_ERROR_ICON)),
        idle_icon_(ui::ResourceBundle::GetSharedInstance().
            GetImageNamed(IDR_MEDIA_ROUTER_IDLE_ICON)),
        warning_icon_(ui::ResourceBundle::GetSharedInstance().
            GetImageNamed(IDR_MEDIA_ROUTER_WARNING_ICON)) {
  }

  ~MediaRouterActionUnitTest() override {}

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    action_.reset(new MediaRouterAction(browser()));
  }

  void TearDown() override {
    action_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  MediaRouterAction* action() { return action_.get(); }
  const media_router::Issue* fake_issue_notification() {
    return &fake_issue_notification_;
  }
  const media_router::Issue* fake_issue_warning() {
    return &fake_issue_warning_;
  }
  const media_router::Issue* fake_issue_fatal() {
    return &fake_issue_fatal_;
  }
  const media_router::MediaRoute fake_route_local() {
    return fake_route_local_;
  }
  const media_router::MediaRoute fake_route_remote() {
    return fake_route_remote_;
  }
  const gfx::Image active_icon() { return active_icon_; }
  const gfx::Image error_icon() { return error_icon_; }
  const gfx::Image idle_icon() { return idle_icon_; }
  const gfx::Image warning_icon() { return warning_icon_; }

 private:
  scoped_ptr<MediaRouterAction> action_;

  // Fake Issues.
  const media_router::Issue fake_issue_notification_;
  const media_router::Issue fake_issue_warning_;
  const media_router::Issue fake_issue_fatal_;

  // Fake Sinks and Sources, used for the Routes.
  const media_router::MediaSink fake_sink1_;
  const media_router::MediaSink fake_sink2_;
  const media_router::MediaSource fake_source1_;
  const media_router::MediaSource fake_source2_;

  // Fake Routes.
  const media_router::MediaRoute fake_route_local_;
  const media_router::MediaRoute fake_route_remote_;

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
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));
}

// Tests the MediaRouterAction icon based on updates to issues.
TEST_F(MediaRouterActionUnitTest, UpdateIssues) {
  // Initially, there are no issues.
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Don't update |current_icon_| since the issue is only a notification.
  action()->OnIssueUpdated(fake_issue_notification());
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since the issue is a warning.
  action()->OnIssueUpdated(fake_issue_warning());
  EXPECT_TRUE(gfx::test::IsEqual(warning_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since the issue is fatal.
  action()->OnIssueUpdated(fake_issue_fatal());
  EXPECT_TRUE(gfx::test::IsEqual(error_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Clear the issue.
  action()->OnIssueUpdated(nullptr);
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));
}

// Tests the MediaRouterAction state based on updates to routes.
TEST_F(MediaRouterActionUnitTest, UpdateRoutes) {
  scoped_ptr<std::vector<media_router::MediaRoute>> routes(
      new std::vector<media_router::MediaRoute>());

  // Initially, there are no routes.
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since there is a local route.
  routes->push_back(fake_route_local());
  routes->push_back(fake_route_remote());
  action()->OnRoutesUpdated(*routes.get());
  EXPECT_TRUE(gfx::test::IsEqual(active_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since there are no more local routes.
  routes->clear();
  routes->push_back(fake_route_remote());
  action()->OnRoutesUpdated(*routes.get());
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // |current_icon_| stays the same if there are no local routes or no routes.
  routes->clear();
  action()->OnRoutesUpdated(*routes.get());
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));
}

// Tests the MediaRouterAction icon based on updates to both issues and routes.
TEST_F(MediaRouterActionUnitTest, UpdateIssuesAndRoutes) {
  scoped_ptr<std::vector<media_router::MediaRoute>> routes(
      new std::vector<media_router::MediaRoute>());

  // Initially, there are no issues or routes.
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // There is no change in |current_icon_| since notification issues do not
  // update the state.
  action()->OnIssueUpdated(fake_issue_notification());
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Non-local routes also do not have an effect on |current_icon_|.
  routes->push_back(fake_route_remote());
  action()->OnRoutesUpdated(*routes.get());
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since there is a local route.
  routes->clear();
  routes->push_back(fake_route_local());
  action()->OnRoutesUpdated(*routes.get());
  EXPECT_TRUE(gfx::test::IsEqual(active_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_|, with a priority to reflect the warning issue
  // rather than the local route.
  action()->OnIssueUpdated(fake_issue_warning());
  EXPECT_TRUE(gfx::test::IsEqual(warning_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Swapping the local route for a non-local one makes no difference to the
  // |current_icon_|.
  routes->clear();
  routes->push_back(fake_route_remote());
  action()->OnRoutesUpdated(*routes.get());
  EXPECT_TRUE(gfx::test::IsEqual(warning_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| since the issue has been updated to fatal.
  action()->OnIssueUpdated(fake_issue_fatal());
  EXPECT_TRUE(gfx::test::IsEqual(error_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Fatal issues still take precedent over local routes.
  routes->clear();
  routes->push_back(fake_route_local());
  action()->OnRoutesUpdated(*routes.get());
  EXPECT_TRUE(gfx::test::IsEqual(error_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // When the fatal issue is dismissed, |current_icon_| reflects the existing
  // local route.
  action()->OnIssueUpdated(nullptr);
  EXPECT_TRUE(gfx::test::IsEqual(active_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));

  // Update |current_icon_| when the local route is swapped out for a non-local
  // route.
  routes->clear();
  routes->push_back(fake_route_remote());
  action()->OnRoutesUpdated(*routes.get());
  EXPECT_TRUE(gfx::test::IsEqual(idle_icon(),
                                 action()->GetIcon(nullptr, gfx::Size())));
}
