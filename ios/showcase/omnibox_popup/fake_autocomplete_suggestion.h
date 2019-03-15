// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_OMNIBOX_POPUP_FAKE_AUTOCOMPLETE_SUGGESTION_H_
#define IOS_SHOWCASE_OMNIBOX_POPUP_FAKE_AUTOCOMPLETE_SUGGESTION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"

// Fake class implementing AutocompleteSuggestion for Showcase.
@interface FakeAutocompleteSuggestion : NSObject <AutocompleteSuggestion>

@property(nonatomic) BOOL supportsDeletion;
@property(nonatomic) BOOL hasAnswer;
@property(nonatomic) BOOL isURL;
@property(nonatomic) BOOL isAppendable;
@property(nonatomic) BOOL isTabMatch;
@property(nonatomic) NSAttributedString* text;
@property(nonatomic) NSAttributedString* detailText;
@property(nonatomic) NSInteger numberOfLines;
@property(nonatomic) OmniboxSuggestionIconType iconType;
@property(nonatomic) GURL imageURL;

@end

#endif  // IOS_SHOWCASE_OMNIBOX_POPUP_FAKE_AUTOCOMPLETE_SUGGESTION_H_
