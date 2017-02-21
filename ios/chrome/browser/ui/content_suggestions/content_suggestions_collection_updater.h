// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UPDATER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UPDATER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestion.h"

@class CollectionViewItem;
@protocol ContentSuggestionsDataSource;
@class ContentSuggestionsViewController;

// Updater for a CollectionViewController populating it with some items and
// handling the items addition.
@interface ContentSuggestionsCollectionUpdater : NSObject

// Initialize with the |dataSource| used to get the data.
- (instancetype)initWithDataSource:(id<ContentSuggestionsDataSource>)dataSource
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// |collectionViewController| this Updater will update. Needs to be set before
// adding items.
@property(nonatomic, assign)
    ContentSuggestionsViewController* collectionViewController;

// Returns whether the section should use the default, non-card style.
- (BOOL)shouldUseCustomStyleForSection:(NSInteger)section;

// Returns the ContentSuggestionType associated with this item.
- (ContentSuggestionType)contentSuggestionTypeForItem:(CollectionViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UPDATER_H_
