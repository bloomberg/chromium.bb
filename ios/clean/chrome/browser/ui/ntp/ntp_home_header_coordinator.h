// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_COORDINATOR_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/coordinators/browser_coordinator.h"

@protocol ContentSuggestionsHeaderControlling;
@protocol ContentSuggestionsHeaderProvider;
@protocol GoogleLandingConsumer;

@protocol ContentSuggestionsCollectionSynchronizing;
@protocol ContentSuggestionsHeaderViewControllerCommandHandler;
@protocol ContentSuggestionsHeaderViewControllerDelegate;
@protocol ContentSuggestionsViewControllerDelegate;

// Coordinator that runs the header containing the logo of the NTP.
@interface NTPHomeHeaderCoordinator : BrowserCoordinator

@property(nonatomic, weak, nullable)
    id<ContentSuggestionsHeaderViewControllerDelegate>
        delegate;
@property(nonatomic, weak, nullable)
    id<ContentSuggestionsHeaderViewControllerCommandHandler>
        commandHandler;
@property(nonatomic, weak, nullable)
    id<ContentSuggestionsCollectionSynchronizing>
        collectionSynchronizer;

@property(nonatomic, strong, readonly, nullable)
    id<ContentSuggestionsHeaderControlling>
        headerController;
@property(nonatomic, strong, readonly, nullable)
    id<ContentSuggestionsHeaderProvider>
        headerProvider;
@property(nonatomic, strong, readonly, nullable) id<GoogleLandingConsumer>
    consumer;
@property(nonatomic, strong, readonly, nullable)
    id<ContentSuggestionsViewControllerDelegate>
        collectionDelegate;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_COORDINATOR_H_
