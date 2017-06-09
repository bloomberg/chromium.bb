// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_DATA_SOURCE_H_

#include <memory>

// TODO(crbug.com/721758): Batch updates should be done in the mediator.
#include "components/reading_list/core/reading_list_model.h"

class GURL;
class ReadingListEntry;
@class CollectionViewItem;
@class ReadingListCollectionViewItem;
@protocol ReadingListDataSink;

// Data Source for the Reading List UI, providing the data sink with the data to
// be displayed. Handle the interactions with the model.
@protocol ReadingListDataSource

// The data sink associated with this data source.
@property(nonatomic, weak, nullable) id<ReadingListDataSink> dataSink;
// Whether the data source is ready to be used.
- (BOOL)ready;
// Whether the data source has some elements.
- (BOOL)hasElements;
// Whether the data source has some read elements.
- (BOOL)hasRead;
// Whether the entry corresponding to the |item| is read.
- (BOOL)isEntryRead:(nonnull CollectionViewItem*)item;

// Mark all entries as seen and stop sending updates to the data sink.
- (void)dataSinkWillBeDismissed;

// Set the read status of the entry identified with |URL|.
- (void)setReadStatus:(BOOL)read forURL:(const GURL&)URL;

// Removes the entry associated with |URL|.
- (void)removeEntryWithURL:(const GURL&)URL;

// Fills the |readArray| and |unreadArray| with the corresponding items from the
// model. The items are sorted most recent first.
- (void)fillReadItems:
            (nullable NSMutableArray<ReadingListCollectionViewItem*>*)readArray
          unreadItems:(nullable NSMutableArray<ReadingListCollectionViewItem*>*)
                          unreadArray;

// TODO(crbug.com/721758): Return ReadingListItem directly.
- (const ReadingListEntry* _Nullable)entryWithURL:(const GURL&)URL;

// Fetches the |faviconURL| of this |item|, notifies the data sink when
// receiving the favicon.
- (void)fetchFaviconForItem:(nonnull ReadingListCollectionViewItem*)item;

// Prepares the data source for batch updates. The UI is not notified for the
// updates happenning between |-beginBatchUpdates| and |-endBatchUpdates|.
- (void)beginBatchUpdates;
// Notifies the data source that the batch updates are over. After calling this
// function the UI is notified when the model changes.
- (void)endBatchUpdates;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_DATA_SOURCE_H_
