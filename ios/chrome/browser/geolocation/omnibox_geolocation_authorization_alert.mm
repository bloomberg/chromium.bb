// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/geolocation/omnibox_geolocation_authorization_alert.h"

#import <UIKit/UIKit.h>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface OmniboxGeolocationAuthorizationAlert () {
  base::WeakNSProtocol<id<OmniboxGeolocationAuthorizationAlertDelegate>>
      delegate_;
}
@end

@implementation OmniboxGeolocationAuthorizationAlert

- (instancetype)initWithDelegate:
        (id<OmniboxGeolocationAuthorizationAlertDelegate>)delegate {
  self = [super init];
  if (self) {
    delegate_.reset(delegate);
  }
  return self;
}

- (instancetype)init {
  return [self initWithDelegate:nil];
}

- (id<OmniboxGeolocationAuthorizationAlertDelegate>)delegate {
  return delegate_.get();
}

- (void)setDelegate:(id<OmniboxGeolocationAuthorizationAlertDelegate>)delegate {
  delegate_.reset(delegate);
}

- (void)showAuthorizationAlert {
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_LOCATION_AUTHORIZATION_ALERT);
  NSString* cancel = l10n_util::GetNSString(IDS_IOS_LOCATION_USAGE_CANCEL);
  NSString* ok = l10n_util::GetNSString(IDS_OK);

  // Use a UIAlertController to match the style of the iOS system location
  // alert.
  base::WeakNSObject<OmniboxGeolocationAuthorizationAlert> weakSelf(self);
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* defaultAction = [UIAlertAction
      actionWithTitle:ok
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                base::scoped_nsobject<OmniboxGeolocationAuthorizationAlert>
                    strongSelf([weakSelf retain]);
                if (strongSelf) {
                  [[strongSelf delegate]
                      authorizationAlertDidAuthorize:strongSelf];
                }
              }];
  [alert addAction:defaultAction];

  UIAlertAction* cancelAction = [UIAlertAction
      actionWithTitle:cancel
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                base::scoped_nsobject<OmniboxGeolocationAuthorizationAlert>
                    strongSelf([weakSelf retain]);
                if (strongSelf) {
                  [[strongSelf delegate]
                      authorizationAlertDidCancel:strongSelf];
                }
              }];
  [alert addAction:cancelAction];

  [[[[UIApplication sharedApplication] keyWindow] rootViewController]
      presentViewController:alert
                   animated:YES
                 completion:nil];
}

@end
