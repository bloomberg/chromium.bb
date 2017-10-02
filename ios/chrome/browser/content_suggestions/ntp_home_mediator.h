// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_NTP_HOME_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_NTP_HOME_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"

namespace ntp_snippets {
class ContentSuggestionsService;
}

namespace web {
class WebState;
}

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class ContentSuggestionsHeaderViewController;
@class ContentSuggestionsMediator;
@class ContentSuggestionsMetricsRecorder;
@class ContentSuggestionsViewController;
@protocol OmniboxFocuser;
@class NTPHomeMetrics;
@protocol SnackbarCommands;
@protocol UrlLoader;

// Mediator for the NTP Home panel, handling the interactions with the
// suggestions.
@interface NTPHomeMediator
    : NSObject<ContentSuggestionsCommands,
               ContentSuggestionsGestureCommands,
               ContentSuggestionsHeaderViewControllerDelegate>

// The web state associated with this NTP.
@property(nonatomic, assign) web::WebState* webState;
// Dispatcher.
@property(nonatomic, weak) id<BrowserCommands, SnackbarCommands, UrlLoader>
    dispatcher;
// Suggestions service used to get the suggestions.
@property(nonatomic, assign)
    ntp_snippets::ContentSuggestionsService* suggestionsService;
// Recorder for the metrics related to ContentSuggestions.
@property(nonatomic, strong) ContentSuggestionsMetricsRecorder* metricsRecorder;
// Recorder for the metrics related to the NTP.
@property(nonatomic, strong) NTPHomeMetrics* NTPMetrics;
// View Controller displaying the suggestions.
@property(nonatomic, weak)
    ContentSuggestionsViewController* suggestionsViewController;
// Mediator for the ContentSuggestions.
@property(nonatomic, weak) ContentSuggestionsMediator* suggestionsMediator;

// Inits the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_NTP_HOME_MEDIATOR_H_
