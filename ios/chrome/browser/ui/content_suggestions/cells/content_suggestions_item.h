// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

namespace base {
class Time;
}

@class ContentSuggestionsItem;
@class FaviconAttributes;
@class FaviconViewNew;
class GURL;

// Delegate for a ContentSuggestionsItem.
@protocol ContentSuggestionsItemDelegate

// Loads the image associated with this item.
- (void)loadImageForSuggestionItem:(ContentSuggestionsItem*)suggestionItem;

@end

// Item for an article in the suggestions.
@interface ContentSuggestionsItem
    : CollectionViewItem<ContentSuggestionIdentification>

// Initialize an article with a |title|, a |subtitle|, an |image| and the |url|
// to the full article. |type| is the type of the item.
- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                    delegate:(id<ContentSuggestionsItemDelegate>)delegate
                         url:(const GURL&)url NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@property(nonatomic, copy, readonly) NSString* title;
@property(nonatomic, strong) UIImage* image;
@property(nonatomic, readonly, assign) GURL URL;
@property(nonatomic, copy) NSString* publisher;
@property(nonatomic, assign) base::Time publishDate;
@property(nonatomic, weak) id<ContentSuggestionsItemDelegate> delegate;
// Attributes for favicon.
@property(nonatomic, strong) FaviconAttributes* attributes;
// Whether the suggestion has an image associated.
@property(nonatomic, assign) BOOL hasImage;
// Whether the suggestion is available offline. If YES, an icon is displayed.
@property(nonatomic, assign) BOOL availableOffline;

@end

// Corresponding cell for an article in the suggestions.
@interface ContentSuggestionsCell : MDCCollectionViewCell

@property(nonatomic, readonly, strong) UILabel* titleLabel;
// View for displaying the favicon.
@property(nonatomic, readonly, strong) FaviconViewNew* faviconView;
// Whether the image should be displayed.
@property(nonatomic, assign) BOOL displayImage;

// Sets an |image| to illustrate the article, replacing the "no image" icon.
- (void)setContentImage:(UIImage*)image;

// Sets the publisher |name| and |date| and add an icon to signal the offline
// availability if |availableOffline| is YES.
- (void)setAdditionalInformationWithPublisherName:(NSString*)publisherName
                                             date:(base::Time)publishDate
                              offlineAvailability:(BOOL)availableOffline;

// Setst the subtitle text. If |subtitle| is nil, the space taken by the
// subtitle is removed.
- (void)setSubtitleText:(NSString*)subtitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_ITEM_H_
