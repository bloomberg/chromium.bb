// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@class FaviconAttributes;
@class FaviconViewNew;

// Item containing a Most Visited suggestion.
@interface ContentSuggestionsMostVisitedItem
    : CollectionViewItem<ContentSuggestionIdentification>

// Attributes to configure the favicon view.
@property(nonatomic, strong, nonnull) FaviconAttributes* attributes;

// Text for the title and the accessibility label of the cell.
@property(nonatomic, copy, nonnull) NSString* title;

@end

// Associated cell to display a Most Visited tile based on the suggestion.
// It displays the favicon for this Most Visited suggestion and its title.
@interface ContentSuggestionsMostVisitedCell : MDCCollectionViewCell

// FaviconView displaying the favicon.
@property(nonatomic, strong, readonly, nonnull) FaviconViewNew* faviconView;

// Title of the Most Visited.
@property(nonatomic, strong, readonly, nonnull) UILabel* titleLabel;

// Size for a Most Visited tile.
+ (CGSize)defaultSize;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
