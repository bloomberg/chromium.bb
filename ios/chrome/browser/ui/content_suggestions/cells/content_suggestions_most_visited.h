// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_H_

#import <Foundation/Foundation.h>

@class FaviconAttributes;

// Object representing a Most Visited suggestion.
@interface ContentSuggestionsMostVisited : NSObject

// Title of the Most Visited suggestion.
@property(nonatomic, copy, nullable) NSString* title;
// Attributes used to display the favicon of the Most Visited suggestion.
// Setting this to nil displays an empty grey background.
@property(nonatomic, strong, nullable) FaviconAttributes* attributes;

// Returns a Most Visited with the properties set.
+ (nullable ContentSuggestionsMostVisited*)
mostVisitedWithTitle:(nullable NSString*)title
          attributes:(nullable FaviconAttributes*)attributes;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_H_
