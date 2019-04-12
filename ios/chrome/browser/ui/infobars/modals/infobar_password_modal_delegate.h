// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

// Delegate to handle Password Infobar Modal actions.
@protocol InfobarPasswordModalDelegate <InfobarModalDelegate>

// Updates (or saves in case they haven't been previously saved) the |username|
// and |password| of the PasswordManagerInfobarDelegate.
- (void)updateCredentialsWithUsername:(NSString*)username
                             password:(NSString*)password;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_MODAL_DELEGATE_H_
