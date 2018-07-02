// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_accessibility_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar.h"

@class ReadingListCollectionViewController;
@protocol ReadingListListViewControllerDelegate;
@protocol ReadingListDataSource;

// Audience for the ReadingListCollectionViewController
@protocol ReadingListCollectionViewControllerAudience

// Whether the collection has items.
- (void)readingListHasItems:(BOOL)hasItems;

@end

@interface ReadingListCollectionViewController
    : CollectionViewController<ReadingListListItemAccessibilityDelegate,
                               ReadingListToolbarActions>

- (instancetype)initWithDataSource:(id<ReadingListDataSource>)dataSource
                           toolbar:(ReadingListToolbar*)toolbar
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@property(nonatomic, weak) id<ReadingListListViewControllerDelegate> delegate;
@property(nonatomic, weak) id<ReadingListCollectionViewControllerAudience>
    audience;
@property(nonatomic, weak) id<ReadingListDataSource> dataSource;

// Prepares this view controller to be dismissed.
- (void)willBeDismissed;

// Reloads all the data.
- (void)reloadData;

+ (NSString*)accessibilityIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_CONTROLLER_H_
