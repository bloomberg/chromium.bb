// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/google_landing_consumer.h"

@protocol ContentSuggestionsCollectionSynchronizing;
@protocol ContentSuggestionsHeaderViewControllerCommandHandler;
@protocol ContentSuggestionsHeaderViewControllerDelegate;
@class NTPHomeHeaderViewController;

@interface NTPHomeHeaderMediator
    : NSObject<ContentSuggestionsHeaderControlling,
               ContentSuggestionsHeaderProvider,
               ContentSuggestionsViewControllerDelegate,
               GoogleLandingConsumer>

@property(nonatomic, weak) id<ContentSuggestionsHeaderViewControllerDelegate>
    delegate;
@property(nonatomic, weak)
    id<ContentSuggestionsHeaderViewControllerCommandHandler>
        commandHandler;
@property(nonatomic, weak) id<ContentSuggestionsCollectionSynchronizing>
    collectionSynchronizer;

@property(nonatomic, weak) NTPHomeHeaderViewController* headerViewController;

// |YES| if its view is visible.  When set to |NO| various UI updates are
// ignored.
@property(nonatomic, assign) BOOL isShowing;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_MEDIATOR_H_
