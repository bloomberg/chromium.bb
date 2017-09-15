// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_UNAVAILABLE_FEATURE_DIALOGS_UNAVAILABLE_FEATURE_DIALOG_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_UNAVAILABLE_FEATURE_DIALOGS_UNAVAILABLE_FEATURE_DIALOG_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"

@protocol UnavailableFeatureDialogDismissalCommands;

// Class responsible for setting up a DialogConsumer for JavaScript dialogs.
@interface UnavailableFeatureDialogMediator : DialogMediator

// Designated initializer for a mediator that uses |request| to configure
// consumers and |dispatcher| for dismissal.
- (nullable instancetype)initWithFeatureName:(nonnull NSString*)featureName
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// The dispatcher.
@property(nonatomic, weak, nullable)
    id<UnavailableFeatureDialogDismissalCommands>
        dispatcher;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_UNAVAILABLE_FEATURE_DIALOGS_UNAVAILABLE_FEATURE_DIALOG_MEDIATOR_H_
