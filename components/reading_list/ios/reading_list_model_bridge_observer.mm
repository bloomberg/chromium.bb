// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/reading_list/ios/reading_list_model_bridge_observer.h"

#include "components/reading_list/ios/reading_list_entry.h"
#include "components/reading_list/ios/reading_list_model.h"

ReadingListModelBridge::ReadingListModelBridge(
    id<ReadingListModelBridgeObserver> observer,
    ReadingListModel* model)
    : observer_(observer), model_(model) {
  DCHECK(model);
  model_->AddObserver(this);
}

ReadingListModelBridge::~ReadingListModelBridge() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void ReadingListModelBridge::ReadingListModelLoaded(
    const ReadingListModel* model) {
  [observer_ readingListModelLoaded:model];
}

void ReadingListModelBridge::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  if ([observer_ respondsToSelector:@selector(readingListModelBeingDeleted:)]) {
    [observer_ readingListModelBeingDeleted:model];
  }
  model_ = nullptr;
}

void ReadingListModelBridge::ReadingListWillRemoveUnreadEntry(
    const ReadingListModel* model,
    size_t index) {
  if ([observer_ respondsToSelector:@selector(readingListModel:
                                        willRemoveUnreadEntryAtIndex:)]) {
    [observer_ readingListModel:model willRemoveUnreadEntryAtIndex:index];
  }
}

void ReadingListModelBridge::ReadingListWillRemoveReadEntry(
    const ReadingListModel* model,
    size_t index) {
  if ([observer_ respondsToSelector:@selector(readingListModel:
                                        willRemoveReadEntryAtIndex:)]) {
    [observer_ readingListModel:model willRemoveReadEntryAtIndex:index];
  }
}

void ReadingListModelBridge::ReadingListWillAddReadEntry(
    const ReadingListModel* model,
    const ReadingListEntry& entry) {
  if ([observer_
          respondsToSelector:@selector(readingListModel:willAddReadEntry:)]) {
    [observer_ readingListModel:model willAddReadEntry:entry];
  }
}

void ReadingListModelBridge::ReadingListWillAddUnreadEntry(
    const ReadingListModel* model,
    const ReadingListEntry& entry) {
  if ([observer_
          respondsToSelector:@selector(readingListModel:willAddUnreadEntry:)]) {
    [observer_ readingListModel:model willAddUnreadEntry:entry];
  }
}

void ReadingListModelBridge::ReadingListDidApplyChanges(
    ReadingListModel* model) {
  [observer_ readingListModelDidApplyChanges:model];
}

void ReadingListModelBridge::ReadingListModelBeganBatchUpdates(
    const ReadingListModel* model) {
  if ([observer_
          respondsToSelector:@selector(readingListModelBeganBatchUpdates:)]) {
    [observer_ readingListModelBeganBatchUpdates:model];
  }
}

void ReadingListModelBridge::ReadingListModelCompletedBatchUpdates(
    const ReadingListModel* model) {
  if ([observer_
          respondsToSelector:@selector(
                                 readingListModelCompletedBatchUpdates:)]) {
    [observer_ readingListModelCompletedBatchUpdates:model];
  }
}

void ReadingListModelBridge::ReadingListWillMoveEntry(
    const ReadingListModel* model,
    size_t index,
    bool read) {
  if ([observer_ respondsToSelector:@selector(readingListModel:
                                                 willMoveEntry:
                                                        isRead:)]) {
    [observer_ readingListModel:model willMoveEntry:index isRead:read];
  }
}

void ReadingListModelBridge::ReadingListWillUpdateUnreadEntry(
    const ReadingListModel* model,
    size_t index) {
  if ([observer_ respondsToSelector:@selector(readingListModel:
                                        willUpdateUnreadEntryAtIndex:)]) {
    [observer_ readingListModel:model willUpdateUnreadEntryAtIndex:index];
  }
}

void ReadingListModelBridge::ReadingListWillUpdateReadEntry(
    const ReadingListModel* model,
    size_t index) {
  if ([observer_ respondsToSelector:@selector(readingListModel:
                                        willUpdateReadEntryAtIndex:)]) {
    [observer_ readingListModel:model willUpdateReadEntryAtIndex:index];
  }
}
