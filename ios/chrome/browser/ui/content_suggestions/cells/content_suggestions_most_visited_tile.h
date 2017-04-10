// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_TILE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_TILE_H_

#import <UIKit/UIKit.h>

@class FaviconAttributes;
@class FaviconViewNew;

// View used to display a Most Visited tile.
@interface ContentSuggestionsMostVisitedTile : UIView

// Width of a Most Visited tile.
+ (CGFloat)width;

// Creates a tile with a |title| and its favicon view configured with
// |attributes|.
+ (nullable ContentSuggestionsMostVisitedTile*)
tileWithTitle:(nullable NSString*)title
   attributes:(nullable FaviconAttributes*)attributes;

// FaviconView displaying the favicon.
@property(nonatomic, strong, readonly, nonnull) FaviconViewNew* faviconView;

// Sets the text and the accessibility label of this view.
- (void)setTitle:(nullable NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_TILE_H_
