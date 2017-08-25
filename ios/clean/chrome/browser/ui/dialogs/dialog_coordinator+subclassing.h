// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_COORDINATOR_SUBCLASSING_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_COORDINATOR_SUBCLASSING_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator.h"

@class DialogMediator;

// Interface used to expose DialogCoordinator configuration to subclasses.
@interface DialogCoordinator (DialogCoordinatorSubclassing)

// The type of dialog UI that should be started for this coordinator.  Defaults
// to UIAlertControllerStyleAlert.
@property(nonatomic, readonly) UIAlertControllerStyle alertStyle;

// The mediator that will be used to configure the DialogConsumer.  The value is
// expected to return non-nil before DialogCoordinator's |-start| is called.
@property(nonatomic, readonly) DialogMediator* mediator;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_COORDINATOR_SUBCLASSING_H_
