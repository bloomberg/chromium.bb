// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarCancelButton;

@interface SyncEncryptionPassphraseTestCase : ChromeTestCase
@end

@implementation SyncEncryptionPassphraseTestCase

// Tests to open the sync passphrase view, and to close it.
- (void)testShowSyncPassphraseAndDismiss {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:@"hello"];
  // Signin.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey openNewTab];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_SYNC_ENTER_PASSPHRASE)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  // Wait until the settings is fully removed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

@end
