// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_CONTENT_SUGGESTIONS_SC_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
#define IOS_SHOWCASE_CONTENT_SUGGESTIONS_SC_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"

@class FaviconAttributes;

// Content Suggestions item configuring a ContentSuggetionsMostVisitedCell.
@interface SCContentSuggestionsMostVisitedItem
    : CollectionViewItem<SuggestedContent>

@property(nonatomic, copy) NSString* title;
@property(nonatomic, strong) FaviconAttributes* attributes;

@end

#endif  // IOS_SHOWCASE_CONTENT_SUGGESTIONS_SC_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
