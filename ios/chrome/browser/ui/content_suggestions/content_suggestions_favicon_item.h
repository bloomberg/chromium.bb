// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Delegate for the ContentSuggestionsFaviconCell, handling the interaction with
// the inner collection view.
@protocol ContentSuggestionsFaviconCellDelegate

// Opens the favicon associated with the |indexPath| of the internal cell.
- (void)openFaviconAtIndexPath:(nonnull NSIndexPath*)innerIndexPath;

@end

// Item for a cell containing favicons in the suggestions. The favicons are
// presented in an inner collection view contained in the cell associated with
// this item. The collection view scrolls horizontally.
@interface ContentSuggestionsFaviconItem : CollectionViewItem

@property(nonatomic, weak, nullable) id<ContentSuggestionsFaviconCellDelegate>
    delegate;

// Adds a favicon with a |favicon| image and a |title| to this item.
- (void)addFavicon:(nonnull UIImage*)favicon withTitle:(nonnull NSString*)title;

@end

// The corresponding cell of a favicon item.
@interface ContentSuggestionsFaviconCell : MDCCollectionViewCell

// The inner collection view, used to display the favicons.
@property(nonatomic, strong, nullable) UICollectionView* collectionView;

@property(nonatomic, weak, nullable) id<ContentSuggestionsFaviconCellDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_ITEM_H_
