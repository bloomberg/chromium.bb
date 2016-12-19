// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_DETAILS_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_DETAILS_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_collection_view_controller.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill
@protocol ReauthenticationProtocol;

@protocol PasswordDetailsCollectionViewControllerDelegate<NSObject>

- (void)deletePassword:(const autofill::PasswordForm&)passwordForm;

@end

@interface PasswordDetailsCollectionViewController
    : SettingsRootCollectionViewController

// The designated initializer. |delegate| and |reauthenticaionModule| must not
// be nil.
- (instancetype)
  initWithPasswordForm:(autofill::PasswordForm)passwordForm
              delegate:
                  (id<PasswordDetailsCollectionViewControllerDelegate>)delegate
reauthenticationModule:(id<ReauthenticationProtocol>)reauthenticationModule
              username:(NSString*)username
              password:(NSString*)password
                origin:(NSString*)origin NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_DETAILS_COLLECTION_VIEW_CONTROLLER_H_
