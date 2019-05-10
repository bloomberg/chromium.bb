// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ALERT_VIEW_CONTROLLER_ALERT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_ALERT_VIEW_CONTROLLER_ALERT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/alert_view_controller/alert_consumer.h"

// This class is a replacement for UIAlertAction.
// Current limitations:
//     Actions Styles are not supported.
@interface AlertAction : NSObject

// The title for this action.
@property(nonatomic, readonly) NSString* title;

// The style for this action. Matches UIAlertAction style.
@property(nonatomic, readonly) UIAlertActionStyle style;

// Initializes an action with |title| and |handler|.
+ (instancetype)actionWithTitle:(NSString*)title
                          style:(UIAlertActionStyle)style
                        handler:(void (^)(AlertAction* action))handler;

- (instancetype)init NS_UNAVAILABLE;

@end

// This class is a replacement for UIAlertController that supports custom
// presentation styles, i.e. change modalPresentationStyle,
// modalTransitionStyle, or transitioningDelegate. The style is more similar to
// the rest of Chromium. Current limitations:
//     Action Sheet Style is not supported.
//     Text fields are not supported.
@interface AlertViewController : UIViewController <AlertConsumer>

// The text in the text fields after presentation.
@property(nonatomic, readonly) NSArray<NSString*>* textFieldResults;

@end

#endif  // IOS_CHROME_BROWSER_UI_ALERT_VIEW_CONTROLLER_ALERT_VIEW_CONTROLLER_H_
