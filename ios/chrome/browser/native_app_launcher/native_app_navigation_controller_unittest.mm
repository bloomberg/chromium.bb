// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#import "ios/chrome/browser/installation_notifier.h"
#include "ios/chrome/browser/native_app_launcher/native_app_infobar_delegate.h"
#import "ios/chrome/browser/native_app_launcher/native_app_navigation_controller.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_metadata.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_whitelist_manager.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"

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
    manager_.reset([fake_manager retain]);
  }
  ~FakeChromeBrowserProvider() override {}

  id<NativeAppWhitelistManager> GetNativeAppWhitelistManager() const override {
    return manager_;
  }

 private:
  base::scoped_nsprotocol<id<NativeAppWhitelistManager>> manager_;
};

class NativeAppNavigationControllerTest : public ChromeWebTest {
 protected:
  void SetUp() override {
    ChromeWebTest::SetUp();
    controller_.reset([[NativeAppNavigationController alloc]
            initWithWebState:web_state()
        requestContextGetter:GetBrowserState()->GetRequestContext()
                         tab:nil]);

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

  base::scoped_nsobject<NativeAppNavigationController> controller_;

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

TEST_F(NativeAppNavigationControllerTest,
       TestRemovingAppFromListAfterInstallation) {
  NSString* const kMapsAppName = @"Maps";
  NSString* const kMapsAppId = @"1";
  NSString* const kYoutubeAppName = @"Youtube";
  NSString* const kYoutubeAppId = @"2";

  base::scoped_nsobject<InstallationNotifier> installationNotifier(
      [[InstallationNotifier alloc] init]);

  FakeNativeAppWhitelistManager* fakeManager =
      [[[FakeNativeAppWhitelistManager alloc] init] autorelease];
  IOSChromeScopedTestingChromeBrowserProvider provider(
      base::MakeUnique<FakeChromeBrowserProvider>(fakeManager));

  base::scoped_nsobject<FakeNativeAppMetadata> metadataMaps(
      [[FakeNativeAppMetadata alloc] init]);
  [metadataMaps setAppName:kMapsAppName];
  [metadataMaps setAppId:kMapsAppId];
  ASSERT_TRUE(metadataMaps.get());
  NSString* appIdMaps = [metadataMaps appId];
  NSNotification* notificationMaps =
      [NSNotification notificationWithName:kMapsAppName
                                    object:installationNotifier];

  base::scoped_nsobject<FakeNativeAppMetadata> metadataYouTube(
      [[FakeNativeAppMetadata alloc] init]);
  [metadataYouTube setAppName:kYoutubeAppName];
  [metadataYouTube setAppId:kYoutubeAppId];

  ASSERT_TRUE(metadataYouTube.get());
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
