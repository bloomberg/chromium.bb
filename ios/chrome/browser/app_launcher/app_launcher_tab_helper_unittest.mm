// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"

#include <memory>

#include "base/compiler_specific.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper_delegate.h"
#import "ios/chrome/browser/web/app_launcher_abuse_detector.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// An object that conforms to AppLauncherTabHelperDelegate for testing.
@interface FakeAppLauncherTabHelperDelegate
    : NSObject<AppLauncherTabHelperDelegate>
// URL of the last launched application.
@property(nonatomic, assign) GURL lastLaunchedAppURL;
// Number of times an app was launched.
@property(nonatomic, assign) NSUInteger countOfAppsLaunched;
// Number of times the repeated launches alert has been shown.
@property(nonatomic, assign) NSUInteger countOfAlertsShown;
// Simulates the user tapping the accept button when prompted via
// |-appLauncherTabHelper:showAlertOfRepeatedLaunchesWithCompletionHandler|.
@property(nonatomic, assign) BOOL simulateUserAcceptingPrompt;
@end

@implementation FakeAppLauncherTabHelperDelegate
@synthesize lastLaunchedAppURL = _lastLaunchedAppURL;
@synthesize countOfAppsLaunched = _countOfAppsLaunched;
@synthesize countOfAlertsShown = _countOfAlertsShown;
@synthesize simulateUserAcceptingPrompt = _simulateUserAcceptingPrompt;
- (BOOL)appLauncherTabHelper:(AppLauncherTabHelper*)tabHelper
            launchAppWithURL:(const GURL&)URL
                  linkTapped:(BOOL)linkTapped {
  self.countOfAppsLaunched++;
  self.lastLaunchedAppURL = URL;
  return YES;
}
- (void)appLauncherTabHelper:(AppLauncherTabHelper*)tabHelper
    showAlertOfRepeatedLaunchesWithCompletionHandler:
        (ProceduralBlockWithBool)completionHandler {
  self.countOfAlertsShown++;
  completionHandler(self.simulateUserAcceptingPrompt);
}
@end

// An AppLauncherAbuseDetector for testing.
@interface FakeAppLauncherAbuseDetector : AppLauncherAbuseDetector
@property(nonatomic, assign) ExternalAppLaunchPolicy policy;
@end

@implementation FakeAppLauncherAbuseDetector
@synthesize policy = _policy;
- (ExternalAppLaunchPolicy)launchPolicyForURL:(const GURL&)URL
                            fromSourcePageURL:(const GURL&)sourcePageURL {
  return self.policy;
}
@end

namespace {
// A fake NavigationManager to be used by the WebState object for the
// AppLauncher.
class FakeNavigationManager : public web::TestNavigationManager {
 public:
  FakeNavigationManager() = default;

  // web::NavigationManager implementation.
  void DiscardNonCommittedItems() override {}

  DISALLOW_COPY_AND_ASSIGN(FakeNavigationManager);
};

// Test fixture for AppLauncherTabHelper class.
class AppLauncherTabHelperTest : public PlatformTest {
 protected:
  AppLauncherTabHelperTest()
      : abuse_detector_([[FakeAppLauncherAbuseDetector alloc] init]),
        delegate_([[FakeAppLauncherTabHelperDelegate alloc] init]) {
    AppLauncherTabHelper::CreateForWebState(&web_state_, abuse_detector_,
                                            delegate_);
    // Allow is the default policy for this test.
    abuse_detector_.policy = ExternalAppLaunchPolicyAllow;
    web_state_.SetNavigationManager(std::make_unique<FakeNavigationManager>());
    tab_helper_ = AppLauncherTabHelper::FromWebState(&web_state_);
  }

  bool TestShouldAllowRequest(NSString* url_string,
                              bool target_frame_is_main,
                              bool has_user_gesture) WARN_UNUSED_RESULT {
    NSURL* url = [NSURL URLWithString:url_string];
    web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_LINK,
        /*source_url=*/GURL::EmptyGURL(), target_frame_is_main,
        has_user_gesture);
    return tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                           request_info);
  }

  web::TestWebState web_state_;
  FakeAppLauncherAbuseDetector* abuse_detector_ = nil;
  FakeAppLauncherTabHelperDelegate* delegate_ = nil;
  AppLauncherTabHelper* tab_helper_;
};

// Tests that a valid URL launches app.
TEST_F(AppLauncherTabHelperTest, AbuseDetectorPolicyAllowedForValidUrl) {
  abuse_detector_.policy = ExternalAppLaunchPolicyAllow;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(1U, delegate_.countOfAppsLaunched);
  EXPECT_EQ(GURL("valid://1234"), delegate_.lastLaunchedAppURL);
}

// Tests that a valid URL does not launch app when launch policy is to block.
TEST_F(AppLauncherTabHelperTest, AbuseDetectorPolicyBlockedForValidUrl) {
  abuse_detector_.policy = ExternalAppLaunchPolicyBlock;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.countOfAlertsShown);
  EXPECT_EQ(0U, delegate_.countOfAppsLaunched);
}

// Tests that a valid URL shows an alert and launches app when launch policy is
// to prompt and user accepts.
TEST_F(AppLauncherTabHelperTest, ValidUrlPromptUserAccepts) {
  abuse_detector_.policy = ExternalAppLaunchPolicyPrompt;
  delegate_.simulateUserAcceptingPrompt = YES;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/false));

  EXPECT_EQ(1U, delegate_.countOfAlertsShown);
  EXPECT_EQ(1U, delegate_.countOfAppsLaunched);
  EXPECT_EQ(GURL("valid://1234"), delegate_.lastLaunchedAppURL);
}

// Tests that a valid URL does not launch app when launch policy is to prompt
// and user rejects.
TEST_F(AppLauncherTabHelperTest, ValidUrlPromptUserRejects) {
  abuse_detector_.policy = ExternalAppLaunchPolicyPrompt;
  delegate_.simulateUserAcceptingPrompt = NO;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.countOfAppsLaunched);
}

// Tests that ShouldAllowRequest only launches apps for App Urls in main frame,
// or iframe when there was a recent user interaction.
TEST_F(AppLauncherTabHelperTest, ShouldAllowRequestWithAppUrl) {
  NSString* url_string = @"itms-apps://itunes.apple.com/us/app/appname/id123";
  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(1U, delegate_.countOfAppsLaunched);

  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/true));
  EXPECT_EQ(2U, delegate_.countOfAppsLaunched);

  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(2U, delegate_.countOfAppsLaunched);

  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/false,
                                      /*has_user_gesture=*/true));
  EXPECT_EQ(3U, delegate_.countOfAppsLaunched);
}

// Tests that ShouldAllowRequest always allows requests and does not launch
// apps for non App Urls.
TEST_F(AppLauncherTabHelperTest, ShouldAllowRequestWithNonAppUrl) {
  EXPECT_TRUE(TestShouldAllowRequest(
      @"http://itunes.apple.com/us/app/appname/id123",
      /*target_frame_is_main=*/true, /*has_user_gesture=*/false));
  EXPECT_TRUE(TestShouldAllowRequest(@"file://a/b/c",
                                     /*target_frame_is_main=*/true,
                                     /*has_user_gesture=*/true));
  EXPECT_TRUE(TestShouldAllowRequest(@"about://test",
                                     /*target_frame_is_main=*/false,
                                     /*has_user_gesture=*/false));
  EXPECT_TRUE(TestShouldAllowRequest(@"data://test",
                                     /*target_frame_is_main=*/false,
                                     /*has_user_gesture=*/true));
  EXPECT_EQ(0U, delegate_.countOfAppsLaunched);
}

// Tests that invalid Urls are completely blocked.
TEST_F(AppLauncherTabHelperTest, InvalidUrls) {
  EXPECT_FALSE(TestShouldAllowRequest(@"",
                                      /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/false));
  EXPECT_FALSE(TestShouldAllowRequest(@"invalid",
                                      /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.countOfAppsLaunched);
}

// Tests that URLs with schemes that might be a security risk are blocked.
TEST_F(AppLauncherTabHelperTest, InsecureUrls) {
  EXPECT_FALSE(TestShouldAllowRequest(@"app-settings://",
                                      /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/false));
  EXPECT_FALSE(TestShouldAllowRequest(@"u2f-x-callback://test",
                                      /*target_frame_is_main=*/true,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.countOfAppsLaunched);
}
}  // namespace
