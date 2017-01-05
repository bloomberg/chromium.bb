// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/native_apps_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/native_apps_collection_view_controller_private.h"

#include <memory>

#include "base/compiler_specific.h"
#import "base/ios/block_types.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/native_app_item.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_metadata.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_whitelist_manager.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util.h"

@interface NativeAppsCollectionViewController (Testing)
@property(nonatomic, retain) NSArray* appsInSettings;
@property(nonatomic, assign) id<StoreKitLauncher> storeKitLauncher;
- (void)configureWithNativeAppWhiteListManager:
    (id<NativeAppWhitelistManager>)nativeAppWhitelistManager;
- (void)autoOpenInAppChanged:(UISwitch*)switchControl;
- (void)installApp:(UIButton*)button;
- (void)recordUserAction:(settings::NativeAppsAction)action;
- (void)appDidInstall:(NSNotification*)note;
- (id<NativeAppMetadata>)nativeAppAtIndex:(NSUInteger)index;
@end

@interface MockNativeAppWhitelistManager : FakeNativeAppWhitelistManager
@end

@implementation MockNativeAppWhitelistManager

- (id)init {
  self = [super init];
  if (self) {
    base::scoped_nsobject<FakeNativeAppMetadata> app1(
        [[FakeNativeAppMetadata alloc] init]);
    [app1 setAppName:@"App1"];
    [app1 setAppId:@"1"];
    [app1 setGoogleOwnedApp:YES];

    base::scoped_nsobject<FakeNativeAppMetadata> app2(
        [[FakeNativeAppMetadata alloc] init]);
    [app2 setAppName:@"App2"];
    [app2 setAppId:@"2"];
    [app2 setGoogleOwnedApp:YES];

    base::scoped_nsobject<FakeNativeAppMetadata> app3(
        [[FakeNativeAppMetadata alloc] init]);
    [app3 setAppName:@"App3"];
    [app3 setAppId:@"3"];
    [app3 setGoogleOwnedApp:YES];

    base::scoped_nsobject<FakeNativeAppMetadata> notOwnedApp(
        [[FakeNativeAppMetadata alloc] init]);
    [notOwnedApp setAppName:@"NotOwnedApp"];
    [notOwnedApp setAppId:@"999"];
    [notOwnedApp setGoogleOwnedApp:NO];

    [self setAppList:@[ app1, app2, notOwnedApp, app3 ]
               tldList:nil
        acceptStoreIDs:nil];
  }
  return self;
}

@end

@interface MockStoreKitLauncher : NSObject<StoreKitLauncher>
@end

@implementation MockStoreKitLauncher
- (void)openAppStore:(id<NativeAppMetadata>)metadata {
}
@end

namespace {

class NativeAppsCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  void SetUp() override {
    CollectionViewControllerTest::SetUp();
    request_context_getter_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    NativeAppsCollectionViewController* native_apps_controller =
        static_cast<NativeAppsCollectionViewController*>(controller());

    mock_whitelist_manager_.reset([[MockNativeAppWhitelistManager alloc] init]);
    [native_apps_controller
        configureWithNativeAppWhiteListManager:mock_whitelist_manager_];
  }

  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    DCHECK(request_context_getter_.get());
    return [[NativeAppsCollectionViewController alloc]
        initWithURLRequestContextGetter:request_context_getter_.get()];
  }

  // Runs the block and checks that the |action| (and only the action) has been
  // recorded.
  void ExpectUserActionAfterBlock(settings::NativeAppsAction action,
                                  ProceduralBlock block) {
    std::unique_ptr<base::HistogramTester> histogram_tester(
        new base::HistogramTester());
    block();
    histogram_tester->ExpectUniqueSample("NativeAppLauncher.Settings", action,
                                         1);
  }

  // Adds a mocked app of class MockNativeAppMetadata at the end of the apps'
  // list.
  void AddMockedApp() {
    NativeAppsCollectionViewController* native_apps_controller =
        static_cast<NativeAppsCollectionViewController*>(controller());
    // Add a mock app at the end of the app list.
    NSMutableArray* apps =
        [NSMutableArray arrayWithArray:[native_apps_controller appsInSettings]];
    ASSERT_GT([apps count], 0U);
    base::scoped_nsobject<FakeNativeAppMetadata> installed_app(
        [[FakeNativeAppMetadata alloc] init]);
    [installed_app setAppName:@"App4"];
    [installed_app setAppId:@"4"];
    [installed_app setGoogleOwnedApp:YES];
    [installed_app resetInfobarHistory];
    [installed_app unsetShouldAutoOpenLinks];
    [installed_app setInstalled:YES];
    [apps addObject:installed_app];
    [native_apps_controller setAppsInSettings:apps];
  }

  // Checks that the object's state is consistent with the represented app
  // metadata at |index| in |section|.
  void CheckNativeAppItem(NSUInteger section, NSUInteger index) {
    NativeAppsCollectionViewController* native_apps_controller =
        static_cast<NativeAppsCollectionViewController*>(controller());
    id<NativeAppMetadata> metadata =
        [native_apps_controller nativeAppAtIndex:index];
    NativeAppItem* item = GetCollectionViewItem(section, index);
    EXPECT_TRUE(item);
    EXPECT_NSEQ([metadata appName], item.name);
    if ([metadata isInstalled]) {
      if ([metadata shouldAutoOpenLinks])
        EXPECT_EQ(NativeAppItemSwitchOn, item.state);
      else
        EXPECT_EQ(NativeAppItemSwitchOff, item.state);
    } else {
      EXPECT_EQ(NativeAppItemInstall, item.state);
    }
  }

  web::TestWebThreadBundle thread_bundle_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  base::scoped_nsobject<MockNativeAppWhitelistManager> mock_whitelist_manager_;
};

// Tests the integrity of the loaded model: section titles, sections and rows,
// along with checking that objects are correctly set up and appear in the
// correct order.
TEST_F(NativeAppsCollectionViewControllerTest, TestModel) {
  NativeAppsCollectionViewController* native_apps_controller =
      static_cast<NativeAppsCollectionViewController*>(controller());
  CheckController();
  EXPECT_EQ(2, NumberOfSections());

  // As many rows as there are apps.
  NSInteger apps_count = [[native_apps_controller appsInSettings] count];
  EXPECT_EQ(apps_count, NumberOfItemsInSection(1));
  for (NSInteger i = 0; i < apps_count; i++) {
    CheckNativeAppItem(1, i);
  }
}

// Tests that native app metadata are correctly updated when the user toggles
// switches. It checks that the appropriate UMA is sent for this user action.
TEST_F(NativeAppsCollectionViewControllerTest, AutoOpenInAppChanged) {
  NativeAppsCollectionViewController* native_apps_controller =
      static_cast<NativeAppsCollectionViewController*>(controller());
  AddMockedApp();
  // Make sure the last app is installed.
  NSInteger last_index = [[native_apps_controller appsInSettings] count] - 1;
  id<NativeAppMetadata> last_app =
      [native_apps_controller nativeAppAtIndex:last_index];
  ASSERT_TRUE([last_app isInstalled]);

  EXPECT_FALSE([last_app shouldAutoOpenLinks]);
  UISwitch* switch_from_cell = [[[UISwitch alloc] init] autorelease];
  switch_from_cell.on = YES;
  switch_from_cell.tag = kTagShift + last_index;

  ExpectUserActionAfterBlock(settings::kNativeAppsActionTurnedAutoOpenOn, ^{
    [native_apps_controller autoOpenInAppChanged:switch_from_cell];
    EXPECT_TRUE([last_app shouldAutoOpenLinks]);
  });

  switch_from_cell.on = NO;

  ExpectUserActionAfterBlock(settings::kNativeAppsActionTurnedAutoOpenOff, ^{
    [native_apps_controller autoOpenInAppChanged:switch_from_cell];
    EXPECT_FALSE([last_app shouldAutoOpenLinks]);
  });
}

// Tests that the App Store is launched when the user clicks on an Install
// button. It checks that the appropriate UMA is sent for this user action.
TEST_F(NativeAppsCollectionViewControllerTest, InstallApp) {
  NativeAppsCollectionViewController* native_apps_controller =
      static_cast<NativeAppsCollectionViewController*>(controller());
  id<StoreKitLauncher> real_opener = [native_apps_controller storeKitLauncher];
  [native_apps_controller
      setStoreKitLauncher:[[[MockStoreKitLauncher alloc] init] autorelease]];
  UIButton* button_from_cell = [UIButton buttonWithType:UIButtonTypeCustom];
  button_from_cell.tag = kTagShift;
  id mock_button = [OCMockObject partialMockForObject:button_from_cell];
  ExpectUserActionAfterBlock(settings::kNativeAppsActionClickedInstall, ^{
    [native_apps_controller installApp:mock_button];
  });

  [mock_button verify];

  [native_apps_controller setStoreKitLauncher:real_opener];
}

// Tests that native app metadata are correctly updated when the related app has
// been installed.
TEST_F(NativeAppsCollectionViewControllerTest, AppDidInstall) {
  NativeAppsCollectionViewController* native_apps_controller =
      static_cast<NativeAppsCollectionViewController*>(controller());
  AddMockedApp();
  // Make sure the last app can open any URL.
  id<NativeAppMetadata> last_app =
      [[native_apps_controller appsInSettings] lastObject];
  ASSERT_TRUE([last_app canOpenURL:GURL()]);

  EXPECT_FALSE([last_app shouldAutoOpenLinks]);
  [native_apps_controller
      appDidInstall:[NSNotification notificationWithName:@"App4" object:nil]];
  EXPECT_TRUE([last_app shouldAutoOpenLinks]);

  [last_app unsetShouldAutoOpenLinks];
}

}  // namespace
