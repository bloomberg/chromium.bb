// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

#include <string>

#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar.h"

namespace favicon {
class LargeIconService;
}

class GURL;
@class ReadingListCollectionViewController;
@class ReadingListCollectionViewItem;
@protocol ReadingListDataSource;
class ReadingListModel;
@class TabModel;

// Audience for the ReadingListCollectionViewController
@protocol ReadingListCollectionViewControllerAudience

// Whether the collection has items.
- (void)readingListHasItems:(BOOL)hasItems;

@end

// Delegate for the ReadingListCollectionViewController, managing the visibility
// of the toolbar, dismissing the Reading List View and opening elements.
@protocol ReadingListCollectionViewControllerDelegate<NSObject>

// Dismisses the Reading List View.
- (void)dismissReadingListCollectionViewController:
    (ReadingListCollectionViewController*)readingListCollectionViewController;

// Displays the context menu for the |readingListItem|. The |menuLocation| is
// the anchor of the context menu in the
// readingListCollectionViewController.collectionView coordinates.
- (void)readingListCollectionViewController:
            (ReadingListCollectionViewController*)
                readingListCollectionViewController
                  displayContextMenuForItem:
                      (ReadingListCollectionViewItem*)readingListItem
                                    atPoint:(CGPoint)menuLocation;

// Opens the entry corresponding to the |readingListItem|.
- (void)
readingListCollectionViewController:
    (ReadingListCollectionViewController*)readingListCollectionViewController
                           openItem:
                               (ReadingListCollectionViewItem*)readingListItem;

// Opens |URL| in a new tab |incognito| or not.
- (void)readingListCollectionViewController:
            (ReadingListCollectionViewController*)
                readingListCollectionViewController
                          openNewTabWithURL:(const GURL&)URL
                                  incognito:(BOOL)incognito;

// Opens the offline url |offlineURL| of the entry saved in the reading list
// model with the |entryURL| url.
- (void)readingListCollectionViewController:
            (ReadingListCollectionViewController*)
                readingListCollectionViewController
                             openOfflineURL:(const GURL&)offlineURL
                      correspondingEntryURL:(const GURL&)entryURL;

@end

@interface ReadingListCollectionViewController
    : CollectionViewController<ReadingListToolbarActions>

- (instancetype)initWithDataSource:(id<ReadingListDataSource>)dataSource
                  largeIconService:(favicon::LargeIconService*)largeIconService
                           toolbar:(ReadingListToolbar*)toolbar
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@property(nonatomic, weak) id<ReadingListCollectionViewControllerDelegate>
    delegate;
@property(nonatomic, weak) id<ReadingListCollectionViewControllerAudience>
    audience;
@property(nonatomic, weak) id<ReadingListDataSource> dataSource;

@property(nonatomic, assign, readonly)
    favicon::LargeIconService* largeIconService;

// Prepares this view controller to be dismissed.
- (void)willBeDismissed;

// Reloads all the data from the ReadingListModel.
- (void)reloadData;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_CONTROLLER_H_
