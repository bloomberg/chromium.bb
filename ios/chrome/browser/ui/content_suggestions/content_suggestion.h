// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTION_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"

namespace base {
class Time;
}

class GURL;

// Enum defining the type of a ContentSuggestions.
typedef NS_ENUM(NSInteger, ContentSuggestionType) {
  // Use this type to pass information about an empty section. Suggestion of
  // this type are empty and should not be displayed. The informations to be
  // displayed are contained in the SectionInfo.
  ContentSuggestionTypeEmpty,
  ContentSuggestionTypeArticle,
  ContentSuggestionTypeReadingList,
  ContentSuggestionTypeMostVisited,
};

// Data for a suggestions item, compatible with Objective-C. Mostly acts as a
// wrapper for ntp_snippets::ContentSuggestion.
@interface ContentSuggestion : NSObject<ContentSuggestionIdentification>

// Title of the suggestion.
@property(nonatomic, copy, nullable) NSString* title;
// Text for the suggestion.
@property(nonatomic, copy, nullable) NSString* text;
// URL associated with the suggestion.
@property(nonatomic, assign) GURL url;
// The name of the publisher.
@property(nonatomic, copy, nullable) NSString* publisher;
// The date of publication.
@property(nonatomic, assign) base::Time publishDate;
// Whether the suggestion is available offline.
@property(nonatomic, assign) BOOL availableOffline;

@property(nonatomic, assign) ContentSuggestionType type;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTION_H_
