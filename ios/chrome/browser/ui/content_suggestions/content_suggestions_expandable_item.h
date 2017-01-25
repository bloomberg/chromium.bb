// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_EXPANDABLE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_EXPANDABLE_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/expandable_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@class ContentSuggestionsExpandableCell;

// Delegate for the ContentSuggestionsExpandableCell. The delegate will take
// care of the expansion/collapsing of the cell.
@protocol ContentSuggestionsExpandableCellDelegate

- (void)expandCell:(UICollectionViewCell*)cell;
- (void)collapseCell:(UICollectionViewCell*)cell;

@end

// Item for an expandable article in the suggestions. An expandable article can
// be expanded, displaying more informations/interactions.
@interface SuggestionsExpandableItem : CollectionViewItem<ExpandableItem>

// Init the article with a |title|, a |subtitle| an |image| and some |detail|
// displayed only when the article is expanded. |type| is the type of the item.
- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                       image:(UIImage*)image
                  detailText:(NSString*)detail NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

// Delegate for the configured cells.
@property(nonatomic, weak) id<ContentSuggestionsExpandableCellDelegate>
    delegate;

@end

// Corresponding cell of an expandable article.
@interface ContentSuggestionsExpandableCell : MDCCollectionViewCell

@property(nonatomic, weak) id<ContentSuggestionsExpandableCellDelegate>
    delegate;
@property(nonatomic, readonly, strong) UILabel* titleLabel;
@property(nonatomic, readonly, strong) UILabel* subtitleLabel;
@property(nonatomic, readonly, strong) UILabel* detailLabel;
@property(nonatomic, strong) UIImageView* imageView;

- (void)expand;
- (void)collapse;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_EXPANDABLE_ITEM_H_
