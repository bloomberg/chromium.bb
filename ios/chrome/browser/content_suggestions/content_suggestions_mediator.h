// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_source.h"

namespace ntp_snippets {
class ContentSuggestionsService;
}

// Mediator for ContentSuggestions. Makes the interface between a
// ntp_snippets::ContentSuggestionsService and the Objective-C services using
// its data.
@interface ContentSuggestionsMediator : NSObject<ContentSuggestionsDataSource>

// Initialize the mediator with the |contentService| to mediate.
- (instancetype)initWithContentService:
    (ntp_snippets::ContentSuggestionsService*)contentService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
