// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_CONTENT_SUGGESTIONS_SC_CONTENT_SUGGESTIONS_ITEMS_H_
#define IOS_SHOWCASE_CONTENT_SUGGESTIONS_SC_CONTENT_SUGGESTIONS_ITEMS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"

// Content Suggestions item configuring a ContentSuggetionsCell.
@interface SCContentSuggestionsItem : CollectionViewItem<SuggestedContent>

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* subtitle;
@property(nonatomic, assign) BOOL hasImage;
@property(nonatomic, copy) NSString* publisher;
@property(nonatomic, strong) NSDate* publishDate;
@property(nonatomic, assign) BOOL availableOffline;

@end

#endif  // IOS_SHOWCASE_CONTENT_SUGGESTIONS_SC_CONTENT_SUGGESTIONS_ITEMS_H_
