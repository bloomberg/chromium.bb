// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_CONFIRM_INFOBAR_CONFIRM_INFOBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_CONFIRM_INFOBAR_CONFIRM_INFOBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

class ConfirmInfoBarDelegate;

// TODO(crbug.com/911864): PLACEHOLDER Work in Progress class for the new
// InfobarUI.
@interface ConfirmInfobarViewController : UIViewController <InfobarUIDelegate>

- (instancetype)initWithInfoBarDelegate:(ConfirmInfoBarDelegate*)infoBarDelegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_CONFIRM_INFOBAR_CONFIRM_INFOBAR_VIEW_CONTROLLER_H_
