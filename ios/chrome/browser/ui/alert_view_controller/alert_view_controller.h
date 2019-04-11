// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ALERT_VIEW_CONTROLLER_ALERT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_ALERT_VIEW_CONTROLLER_ALERT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// This class is a replacement for UIAlertAction.
// Current limitations:
//     Actions Styles are not supported.
@interface AlertAction : NSObject

// The title for this action.
@property(nonatomic, readonly) NSString* title;

// Initializes an action with |title| and |handler|.
+ (instancetype)actionWithTitle:(NSString*)title
                        handler:(void (^)(AlertAction* action))handler;

- (instancetype)init NS_UNAVAILABLE;

@end

// This class is a replacement for UIAlertController that supports custom
// presentation styles, i.e. change modalPresentationStyle,
// modalTransitionStyle, or transitioningDelegate. The style is more similar to
// the rest of Chromium. Current limitations:
//     Action Sheet Style is not supported.
//     Text fields are not supported.
@interface AlertViewController : UIViewController

// The title of the alert, will appear at the top and in bold.
@property(nonatomic, copy) NSString* title;

// The message of the alert, will appear after the title.
@property(nonatomic, copy) NSString* message;

// The actions that had been added to this alert.
@property(nonatomic, readonly) NSArray<AlertAction*>* actions;

// The text fields that had been added to this alert.
@property(nonatomic, readonly) NSArray<UITextField*>* textFields;

// Adds an action to the alert.
- (void)addAction:(AlertAction*)action;

// Adds a text field and with an optional block to configure the properties of
// the text field.
- (void)addTextFieldWithConfigurationHandler:
    (void (^)(UITextField* textField))configurationHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_ALERT_VIEW_CONTROLLER_ALERT_VIEW_CONTROLLER_H_
