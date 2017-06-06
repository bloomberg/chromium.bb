// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_

#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_visual_type.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"

class GURL;

// Item containing a Most Visited suggestion.
@interface ContentSuggestionsMostVisitedItem
    : CollectionViewItem<SuggestedContent>

// Text for the title and the accessibility label of the cell.
@property(nonatomic, copy, nonnull) NSString* title;

// URL of the Most Visited.
@property(nonatomic, assign) GURL URL;
// Source of the Most Visited tile.
@property(nonatomic, assign) ntp_tiles::TileSource source;

- (ntp_tiles::TileVisualType)tileType;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
