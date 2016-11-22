
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_READING_LIST_MODEL_BRIDGE_OBSERVER_H_
#define COMPONENTS_READING_LIST_READING_LIST_MODEL_BRIDGE_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "components/reading_list/reading_list_model_observer.h"

// Protocol duplicating all Reading List Model Observer methods in Objective-C.
@protocol ReadingListModelBridgeObserver<NSObject>

@required
- (void)readingListModelLoaded:(const ReadingListModel*)model;
- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model;

@optional
- (void)readingListModel:(const ReadingListModel*)model
    willRemoveUnreadEntryAtIndex:(size_t)index;
- (void)readingListModel:(const ReadingListModel*)model
    willRemoveReadEntryAtIndex:(size_t)index;

- (void)readingListModel:(const ReadingListModel*)model
           willMoveEntry:(size_t)unreadIndex
                  isRead:(BOOL)read;

- (void)readingListModel:(const ReadingListModel*)model
      willAddUnreadEntry:(const ReadingListEntry&)entry;
- (void)readingListModel:(const ReadingListModel*)model
        willAddReadEntry:(const ReadingListEntry&)entry;

- (void)readingListModelBeganBatchUpdates:(const ReadingListModel*)model;
- (void)readingListModelCompletedBatchUpdates:(const ReadingListModel*)model;

- (void)readingListModelBeingDeleted:(const ReadingListModel*)model;

- (void)readingListModel:(const ReadingListModel*)model
    willUpdateUnreadEntryAtIndex:(size_t)index;
- (void)readingListModel:(const ReadingListModel*)model
    willUpdateReadEntryAtIndex:(size_t)index;

@end

// Observer for the Reading List model that translates all the callbacks to
// Objective-C calls.
class ReadingListModelBridge : public ReadingListModelObserver {
 public:
  explicit ReadingListModelBridge(id<ReadingListModelBridgeObserver> observer,
                                  ReadingListModel* model);
  ~ReadingListModelBridge() override;

 private:
  void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) override;

  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override;
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;
  void ReadingListWillRemoveUnreadEntry(const ReadingListModel* model,
                                        size_t index) override;
  void ReadingListWillRemoveReadEntry(const ReadingListModel* model,
                                      size_t index) override;
  void ReadingListWillMoveEntry(const ReadingListModel* model,
                                size_t index,
                                bool read) override;
  void ReadingListWillAddUnreadEntry(const ReadingListModel* model,
                                     const ReadingListEntry& entry) override;
  void ReadingListWillAddReadEntry(const ReadingListModel* model,
                                   const ReadingListEntry& entry) override;
  void ReadingListDidApplyChanges(ReadingListModel* model) override;
  void ReadingListWillUpdateUnreadEntry(const ReadingListModel* model,
                                        size_t index) override;
  void ReadingListWillUpdateReadEntry(const ReadingListModel* model,
                                      size_t index) override;

  __unsafe_unretained id<ReadingListModelBridgeObserver> observer_;
  ReadingListModel* model_;  // weak

  DISALLOW_COPY_AND_ASSIGN(ReadingListModelBridge);
};

#endif  // COMPONENTS_READING_LIST_READING_LIST_MODEL_BRIDGE_OBSERVER_H_
