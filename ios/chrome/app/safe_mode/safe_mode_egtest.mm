// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/safe_mode/safe_mode_view_controller.h"
#import "ios/chrome/browser/ui/main/main_view_controller.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/test/base/scoped_block_swizzler.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the top view controller for rendering the Safe Mode Controller.
UIViewController* GetActiveViewController() {
  UIWindow* mainWindow = [[UIApplication sharedApplication] keyWindow];
  DCHECK([mainWindow isKindOfClass:[ChromeOverlayWindow class]]);
  MainViewController* main_view_controller =
      base::mac::ObjCCast<MainViewController>([mainWindow rootViewController]);
  return main_view_controller.activeViewController;
}

// Verifies that |message| is displayed.
void AssertMessageOnPage(NSString* message) {
  id<GREYMatcher> messageMatcher = [GREYMatchers matcherForText:message];
  [[EarlGrey selectElementWithMatcher:messageMatcher]
      assertWithMatcher:grey_notNil()];
}

// Verifies that |message| is not displayed.
void AssertMessageNotOnPage(NSString* message) {
  id<GREYMatcher> messageMatcher = [GREYMatchers matcherForText:message];
  [[EarlGrey selectElementWithMatcher:messageMatcher]
      assertWithMatcher:grey_nil()];
}

// Verifies that the button to reload chrome is displayed.
void AssertTryAgainButtonOnPage() {
  NSString* tryAgain =
      NSLocalizedString(@"IDS_IOS_SAFE_MODE_RELOAD_CHROME", @"");
  // This is uppercased to match MDC button label convention.
  NSString* tryAgainPrimaryAction =
      [tryAgain uppercaseStringWithLocale:[NSLocale currentLocale]];
  id<GREYMatcher> tryAgainMatcher =
      [GREYMatchers matcherForButtonTitle:tryAgainPrimaryAction];
  [[EarlGrey selectElementWithMatcher:tryAgainMatcher]
      assertWithMatcher:grey_notNil()];
}

}  // namespace

// Expose internal class methods for swizzling.
@interface SafeModeViewController (Testing)
+ (BOOL)detectedThirdPartyMods;
+ (BOOL)hasReportToUpload;
- (NSArray*)startupCrashModules;
@end

// Tests the display of Safe Mode Controller under different error states of
// jailbroken-ness and whether a crash dump was saved.
@interface SafeModeTestCase : ChromeTestCase
@end

@implementation SafeModeTestCase

// Tests that Safe Mode crash upload screen is displayed when there are crash
// reports to upload.
- (void)testSafeModeSendingCrashReport {
  // Mocks the +hasReportToUpload method by swizzling to return positively that
  // there are crash reports to upload.
  ScopedBlockSwizzler hasReport([SafeModeViewController class],
                                @selector(hasReportToUpload), ^{
                                  return YES;
                                });

  // Instantiates a Safe Mode controller and displays it.
  SafeModeViewController* safeModeController =
      [[SafeModeViewController alloc] initWithDelegate:nil];
  [GetActiveViewController() presentViewController:safeModeController
                                          animated:NO
                                        completion:nil];
  // Verifies screen content that shows that crash report is being uploaded.
  AssertMessageOnPage(NSLocalizedString(@"IDS_IOS_SAFE_MODE_AW_SNAP", @""));
  AssertMessageOnPage(
      NSLocalizedString(@"IDS_IOS_SAFE_MODE_UNKNOWN_CAUSE", @""));
  AssertTryAgainButtonOnPage();
  AssertMessageOnPage(
      NSLocalizedString(@"IDS_IOS_SAFE_MODE_SENDING_CRASH_REPORT", @""));
}

// Tests that Safe Mode screen is displayed with a message that there are
// jailbroken mods that caused a crash. Crash reports are not sent.
- (void)testSafeModeDetectedThirdPartyMods {
  // Mocks the +detectedThirdPartyMods method by swizzling to return positively
  // that device appears to be jailbroken and contains third party mods.
  ScopedBlockSwizzler thirdParty([SafeModeViewController class],
                                 @selector(detectedThirdPartyMods), ^{
                                   return YES;
                                 });
  // Returns an empty list to simulate no known mods detected.
  ScopedBlockSwizzler badModules([SafeModeViewController class],
                                 @selector(startupCrashModules), ^{
                                   return @[];
                                 });

  // Instantiates a Safe Mode controller and displays it.
  SafeModeViewController* safeModeController =
      [[SafeModeViewController alloc] initWithDelegate:nil];
  [GetActiveViewController() presentViewController:safeModeController
                                          animated:NO
                                        completion:nil];
  // Verifies screen content that does not show crash report being uploaded.
  // When devices are jailbroken, the crash reports are not very useful.
  AssertMessageOnPage(NSLocalizedString(@"IDS_IOS_SAFE_MODE_AW_SNAP", @""));
  AssertMessageOnPage(
      NSLocalizedString(@"IDS_IOS_SAFE_MODE_TWEAKS_FOUND", @""));
  AssertTryAgainButtonOnPage();
  AssertMessageNotOnPage(
      NSLocalizedString(@"IDS_IOS_SAFE_MODE_SENDING_CRASH_REPORT", @""));
}

// Tests that Safe Mode screen is displayed with a message that there are
// jailbroken mods listing the names of the known to be bad mods that caused a
// crash. Crash reports are not sent.
- (void)testSafeModeBothThirdPartyModsAndHasReport {
  // Mocks the +detectedThirdPartyMods method by swizzling to return positively
  // that device appears to be jailbroken and contains third party mods.
  ScopedBlockSwizzler thirdParty([SafeModeViewController class],
                                 @selector(detectedThirdPartyMods), ^{
                                   return YES;
                                 });
  // Mocked list of bad jailbroken mods. These will be checked later.
  NSArray* badModulesList = @[ @"iAmBad", @"MJackson" ];
  ScopedBlockSwizzler badModules([SafeModeViewController class],
                                 @selector(startupCrashModules), ^{
                                   return badModulesList;
                                 });
  ScopedBlockSwizzler hasReport([SafeModeViewController class],
                                @selector(hasReportToUpload), ^{
                                  return YES;
                                });
  // Instantiates a Safe Mode controller and displays it.
  SafeModeViewController* safeModeController =
      [[SafeModeViewController alloc] initWithDelegate:nil];
  [GetActiveViewController() presentViewController:safeModeController
                                          animated:NO
                                        completion:nil];
  // Verifies screen content that does not show crash report being uploaded.
  // When devices are jailbroken, the crash reports are not very useful.
  AssertMessageOnPage(NSLocalizedString(@"IDS_IOS_SAFE_MODE_AW_SNAP", @""));
  // Constructs the list of bad mods based on |badModulesList| above.
  NSString* message =
      [NSLocalizedString(@"IDS_IOS_SAFE_MODE_NAMED_TWEAKS_FOUND", @"")
          stringByAppendingString:@"\n\n    iAmBad\n    MJackson"];
  AssertMessageOnPage(message);
  AssertTryAgainButtonOnPage();
  AssertMessageNotOnPage(
      NSLocalizedString(@"IDS_IOS_SAFE_MODE_SENDING_CRASH_REPORT", @""));
}

@end
