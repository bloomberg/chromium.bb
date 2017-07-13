// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"

namespace base {
class Time;
}

@class ContentSuggestionsItem;
@class FaviconAttributes;
class GURL;

// Delegate for SuggestedContent.
@protocol ContentSuggestionsItemDelegate

// Loads the image associated with the |suggestedItem|.
- (void)loadImageForSuggestedItem:(ContentSuggestionsItem*)suggestedItem;

@end

// Item for an article in the suggestions.
@interface ContentSuggestionsItem : CollectionViewItem<SuggestedContent>

// Initialize an article with a |title|, a |subtitle|, an |image| and the |url|
// to the full article. |type| is the type of the item.
- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                         url:(const GURL&)url NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@property(nonatomic, weak) id<ContentSuggestionsItemDelegate> delegate;
@property(nonatomic, copy, readonly) NSString* title;
@property(nonatomic, readonly, assign) GURL URL;
@property(nonatomic, copy) NSString* publisher;
@property(nonatomic, assign) base::Time publishDate;
// Image associated with this content.
@property(nonatomic, strong) UIImage* image;
// Whether the suggestion has an image associated.
@property(nonatomic, assign) BOOL hasImage;
// Whether the suggestion is available offline. If YES, an icon is displayed.
@property(nonatomic, assign) BOOL availableOffline;
// Attributes for favicon.
@property(nonatomic, strong) FaviconAttributes* attributes;
// URL for the favicon, if different of |URL|.
@property(nonatomic, assign) GURL faviconURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_ITEM_H_
