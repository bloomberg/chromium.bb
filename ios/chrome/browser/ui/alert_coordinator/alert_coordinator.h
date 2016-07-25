// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ALERT_COORDINATOR_ALERT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_ALERT_COORDINATOR_ALERT_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#import "ios/chrome/browser/chrome_coordinator.h"

// A coordinator specialization for the case where the coordinator is
// creating and managing an alert (popup or action sheet) to be displayed to the
// user. Dismiss it with no animation.
// The type of alert displayed depends on the init called.
// Calling |-stop| on this coordinator destroys the current alert.
@interface AlertCoordinator : ChromeCoordinator

// Whether a cancel button has been added.
@property(nonatomic, readonly) BOOL cancelButtonAdded;
// Message of the alert.
@property(nonatomic, copy) NSString* message;
// Whether the alert is visible. This will be true after |-start| is called
// until a subsequent |-stop|.
@property(nonatomic, readonly, getter=isVisible) BOOL visible;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;

// Init a coordinator for displaying a alert on this view controller. If
// |-configureForActionSheetWithRect:popoverView:| is not called, it will be a
// modal alert.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
    NS_DESIGNATED_INITIALIZER;

// Call this before adding any button to change the alert to an action sheet.
- (void)configureForActionSheetWithRect:(CGRect)rect popoverView:(UIView*)view;

// Adds an item at the end of the menu. It does nothing if |visible| is true or
// if trying to add an item with a UIAlertActionStyleCancel while
// |cancelButtonAdded| is true.
- (void)addItemWithTitle:(NSString*)title
                  action:(ProceduralBlock)actionBlock
                   style:(UIAlertActionStyle)style;

// Returns the number of actions attached to the current alert.
- (NSUInteger)actionsCount;

@end

#endif  // IOS_CHROME_BROWSER_UI_ALERT_COORDINATOR_ALERT_COORDINATOR_H_
