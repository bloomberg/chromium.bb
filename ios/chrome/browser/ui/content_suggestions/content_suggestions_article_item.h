// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ARTICLE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ARTICLE_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestion_identifier.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#include "url/gurl.h"

namespace base {
class Time;
}

@class ContentSuggestionsArticleItem;

// Delegate for a ContentSuggestionsArticleItem.
@protocol ContentSuggestionsArticleItemDelegate

// Loads the image associated with this item.
- (void)loadImageForArticleItem:(ContentSuggestionsArticleItem*)articleItem;

@end

// Item for an article in the suggestions.
@interface ContentSuggestionsArticleItem
    : CollectionViewItem<ContentSuggestionIdentification>

// Initialize an article with a |title|, a |subtitle|, an |image| and the |url|
// to the full article. |type| is the type of the item.
- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                    delegate:(id<ContentSuggestionsArticleItemDelegate>)delegate
                         url:(const GURL&)url NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@property(nonatomic, copy, readonly) NSString* title;
@property(nonatomic, strong) UIImage* image;
@property(nonatomic, readonly, assign) GURL articleURL;
@property(nonatomic, copy) NSString* publisher;
@property(nonatomic, assign) base::Time publishDate;
@property(nonatomic, weak) id<ContentSuggestionsArticleItemDelegate> delegate;

@end

// Corresponding cell for an article in the suggestions.
@interface ContentSuggestionsArticleCell : MDCCollectionViewCell

@property(nonatomic, readonly, strong) UILabel* titleLabel;
@property(nonatomic, readonly, strong) UILabel* subtitleLabel;
@property(nonatomic, readonly, strong) UIImageView* imageView;

- (void)setPublisherName:(NSString*)publisherName date:(base::Time)publishDate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ARTICLE_ITEM_H_
