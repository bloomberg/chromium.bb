// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/content_suggestions_header_provider.h"
#import "ios/chrome/browser/ui/ntp/google_landing_consumer.h"

@protocol ContentSuggestionsHeaderControllerDelegate;
@protocol ContentSuggestionsHeaderControllerCommandHandler;
@protocol OmniboxFocuser;
@protocol UrlLoader;
class ReadingListModel;

// Controller for the header containing the logo and the fake omnibox, handling
// the interactions between the header and the collection, and the rest of the
// application.
@interface ContentSuggestionsHeaderController
    : NSObject<ContentSuggestionsHeaderProvider, GoogleLandingConsumer>

@property(nonatomic, weak) id<UrlLoader, OmniboxFocuser> dispatcher;
@property(nonatomic, weak) id<ContentSuggestionsHeaderControllerDelegate>
    delegate;
@property(nonatomic, weak) id<ContentSuggestionsHeaderControllerCommandHandler>
    commandHandler;
@property(nonatomic, assign) ReadingListModel* readingListModel;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_CONTROLLER_H_
