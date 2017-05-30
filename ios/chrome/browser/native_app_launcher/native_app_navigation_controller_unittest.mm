// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/installation_notifier.h"
#include "ios/chrome/browser/native_app_launcher/native_app_infobar_delegate.h"
#import "ios/chrome/browser/native_app_launcher/native_app_navigation_controller.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_metadata.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_whitelist_manager.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NativeAppNavigationController (Testing)
- (void)recordInfobarDisplayedOfType:(NativeAppControllerType)type
                    onLinkNavigation:(BOOL)isLinkNavigation;
- (NSMutableSet*)appsPossiblyBeingInstalled;
- (void)removeAppFromNotification:(NSNotification*)notification;
@end

namespace {

class FakeChromeBrowserProvider : public ios::TestChromeBrowserProvider {
 public:
  FakeChromeBrowserProvider(FakeNativeAppWhitelistManager* fake_manager) {
    manager_ = fake_manager;
  }
  ~FakeChromeBrowserProvider() override {}

  id<NativeAppWhitelistManager> GetNativeAppWhitelistManager() const override {
    return manager_;
  }

 private:
  id<NativeAppWhitelistManager> manager_;
};

class NativeAppNavigationControllerTest : public ChromeWebTest {
 protected:
  void SetUp() override {
    ChromeWebTest::SetUp();
    controller_ =
        [[NativeAppNavigationController alloc] initWithWebState:web_state()];

    action_callback_ =
        base::Bind(&NativeAppNavigationControllerTest::OnUserAction,
                   base::Unretained(this));
    base::AddActionCallback(action_callback_);

    handler_called_counter_ = 0;
  }

  void TearDown() override {
    base::RemoveActionCallback(action_callback_);
    ChromeWebTest::TearDown();
  }

  void SetExpectedActionName(const std::string& action_name) {
    expected_action_name_.reset(new std::string(action_name));
  }

  void OnUserAction(const std::string& action_name) {
    EXPECT_EQ(*expected_action_name_, action_name);
    handler_called_counter_++;
  }

  void ExpectHandlerCalledAndReset(int number_of_calls) {
    EXPECT_EQ(number_of_calls, handler_called_counter_);
    handler_called_counter_ = 0;
  }

  NativeAppNavigationController* controller_;

  // The callback to invoke when an action is recorded.
  base::ActionCallback action_callback_;
  std::unique_ptr<std::string> expected_action_name_;
  int handler_called_counter_;
};

TEST_F(NativeAppNavigationControllerTest, TestConstructor) {
  EXPECT_TRUE(controller_);
}

TEST_F(NativeAppNavigationControllerTest, TestUMA) {
  SetExpectedActionName("MobileGALInstallInfoBarLinkNavigation");
  [controller_ recordInfobarDisplayedOfType:NATIVE_APP_INSTALLER_CONTROLLER
                           onLinkNavigation:YES];
  ExpectHandlerCalledAndReset(1);

  SetExpectedActionName("MobileGALInstallInfoBarDirectNavigation");
  [controller_ recordInfobarDisplayedOfType:NATIVE_APP_INSTALLER_CONTROLLER
                           onLinkNavigation:NO];
  ExpectHandlerCalledAndReset(1);

  SetExpectedActionName("MobileGALLaunchInfoBar");
  [controller_ recordInfobarDisplayedOfType:NATIVE_APP_LAUNCHER_CONTROLLER
                           onLinkNavigation:YES];
  ExpectHandlerCalledAndReset(1);
  [controller_ recordInfobarDisplayedOfType:NATIVE_APP_LAUNCHER_CONTROLLER
                           onLinkNavigation:NO];
  ExpectHandlerCalledAndReset(1);

  SetExpectedActionName("MobileGALOpenPolicyInfoBar");
  [controller_ recordInfobarDisplayedOfType:NATIVE_APP_OPEN_POLICY_CONTROLLER
                           onLinkNavigation:YES];
  ExpectHandlerCalledAndReset(1);
  [controller_ recordInfobarDisplayedOfType:NATIVE_APP_OPEN_POLICY_CONTROLLER
                           onLinkNavigation:NO];
  ExpectHandlerCalledAndReset(1);
}

// Tests presenting NativeAppInfoBar after page is loaded.
TEST_F(NativeAppNavigationControllerTest, NativeAppInfoBar) {
  SetExpectedActionName("MobileGALInstallInfoBarDirectNavigation");
  InfoBarManagerImpl::CreateForWebState(web_state());

  // Set up fake metadata.
  FakeNativeAppWhitelistManager* fakeManager =
      [[FakeNativeAppWhitelistManager alloc] init];
  IOSChromeScopedTestingChromeBrowserProvider provider(
      base::MakeUnique<FakeChromeBrowserProvider>(fakeManager));
  FakeNativeAppMetadata* metadata = [[FakeNativeAppMetadata alloc] init];
  fakeManager.metadata = metadata;
  metadata.appName = @"App";
  metadata.appId = @"App-ID";

  // Load the page to trigger infobar presentation.
  LoadHtml(@"<html><body></body></html>", GURL("http://test.com"));

  // Verify that infobar was presented
  auto* infobar_manager = InfoBarManagerImpl::FromWebState(web_state());
  ASSERT_EQ(1U, infobar_manager->infobar_count());
  infobars::InfoBar* infobar = infobar_manager->infobar_at(0);
  auto* delegate = infobar->delegate()->AsNativeAppInfoBarDelegate();
  ASSERT_TRUE(delegate);

  // Verify infobar appearance.
  EXPECT_EQ("Open this page in the App app?",
            base::UTF16ToUTF8(delegate->GetInstallText()));
  EXPECT_EQ("Open this page in the App app?",
            base::UTF16ToUTF8(delegate->GetLaunchText()));
  EXPECT_EQ("Open in App", base::UTF16ToUTF8(delegate->GetOpenPolicyText()));
  EXPECT_EQ("Just once", base::UTF16ToUTF8(delegate->GetOpenOnceText()));
  EXPECT_EQ("Always", base::UTF16ToUTF8(delegate->GetOpenAlwaysText()));
  EXPECT_NSEQ(@"App-ID", delegate->GetAppId());
}

TEST_F(NativeAppNavigationControllerTest,
       TestRemovingAppFromListAfterInstallation) {
  NSString* const kMapsAppName = @"Maps";
  NSString* const kMapsAppId = @"1";
  NSString* const kYoutubeAppName = @"Youtube";
  NSString* const kYoutubeAppId = @"2";

  InstallationNotifier* installationNotifier =
      [[InstallationNotifier alloc] init];

  FakeNativeAppWhitelistManager* fakeManager =
      [[FakeNativeAppWhitelistManager alloc] init];
  IOSChromeScopedTestingChromeBrowserProvider provider(
      base::MakeUnique<FakeChromeBrowserProvider>(fakeManager));

  FakeNativeAppMetadata* metadataMaps = [[FakeNativeAppMetadata alloc] init];
  [metadataMaps setAppName:kMapsAppName];
  [metadataMaps setAppId:kMapsAppId];
  ASSERT_TRUE(metadataMaps);
  NSString* appIdMaps = [metadataMaps appId];
  NSNotification* notificationMaps =
      [NSNotification notificationWithName:kMapsAppName
                                    object:installationNotifier];

  FakeNativeAppMetadata* metadataYouTube = [[FakeNativeAppMetadata alloc] init];
  [metadataYouTube setAppName:kYoutubeAppName];
  [metadataYouTube setAppId:kYoutubeAppId];

  ASSERT_TRUE(metadataYouTube);
  NSString* appIdYouTube = [metadataYouTube appId];
  NSNotification* notificationYouTube =
      [NSNotification notificationWithName:kYoutubeAppName
                                    object:installationNotifier];

  DCHECK([[controller_ appsPossiblyBeingInstalled] count] == 0);
  [[controller_ appsPossiblyBeingInstalled] addObject:appIdMaps];
  DCHECK([[controller_ appsPossiblyBeingInstalled] count] == 1);
  [[controller_ appsPossiblyBeingInstalled] addObject:appIdYouTube];
  DCHECK([[controller_ appsPossiblyBeingInstalled] count] == 2);
  [fakeManager setAppScheme:kMapsAppName];
  [controller_ removeAppFromNotification:notificationMaps];
  DCHECK([[controller_ appsPossiblyBeingInstalled] count] == 1);
  [fakeManager setAppScheme:kYoutubeAppName];
  [controller_ removeAppFromNotification:notificationYouTube];
  DCHECK([[controller_ appsPossiblyBeingInstalled] count] == 0);
}

}  // namespace
