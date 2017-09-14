// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_provider.h"
#import "ios/chrome/browser/ui/ntp/google_landing_consumer.h"

@protocol ContentSuggestionsCollectionSynchronizing;
@protocol ContentSuggestionsCommands;
@protocol ContentSuggestionsHeaderViewControllerDelegate;

// Coordinator handling the header of the NTP home panel.
@interface NTPHomeHeaderViewController
    : UIViewController<ChromeBroadcastObserver,
                       ContentSuggestionsHeaderControlling,
                       ContentSuggestionsHeaderProvider,
                       GoogleLandingConsumer>

@property(nonatomic, weak) id<ContentSuggestionsCommands> commandHandler;
@property(nonatomic, weak) id<ContentSuggestionsHeaderViewControllerDelegate>
    delegate;

// Synchronize the interaction with the collection.
@property(nonatomic, weak) id<ContentSuggestionsCollectionSynchronizing>
    collectionSynchronizer;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_VIEW_CONTROLLER_H_
