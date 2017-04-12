// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@class ContentSuggestionsMostVisited;
@class FaviconAttributes;

// Item containing the Most Visited suggestions.
@interface ContentSuggestionsMostVisitedItem
    : CollectionViewItem<ContentSuggestionIdentification>

// Most Visited suggestions for this item.
@property(nonatomic, copy, nonnull)
    NSArray<ContentSuggestionsMostVisited*>* suggestions;

@end

// Associated cell to display the Most Visited suggestions.
// This cell displays the most visited suggestions on two rows, vertically
// stacked. Each row is a horizontal stack of ContentSuggestionsTiles. The
// number of tiles per row depends of the available width.
@interface ContentSuggestionsMostVisitedCell : MDCCollectionViewCell

// Sets the Most Visited suggestions of this cell.
- (void)setSuggestions:
    (nonnull NSArray<ContentSuggestionsMostVisited*>*)suggestions;

// Returns the maximum number of tiles per line, limited to 4.
- (NSUInteger)numberOfTilesPerLine;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
