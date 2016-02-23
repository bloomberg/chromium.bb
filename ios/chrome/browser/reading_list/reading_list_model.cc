// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_model.h"

ReadingListModel::ReadingListModel() : current_batch_updates_count_(0) {}
ReadingListModel::~ReadingListModel() {}

// Observer methods.
void ReadingListModel::AddObserver(ReadingListModelObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (loaded()) {
    observer->ReadingListModelLoaded(this);
  }
}

void ReadingListModel::RemoveObserver(ReadingListModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

// Batch update methods.
bool ReadingListModel::IsPerformingBatchUpdates() const {
  return current_batch_updates_count_ > 0;
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
ReadingListModel::BeginBatchUpdates() {
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> token(
      new ReadingListModel::ScopedReadingListBatchUpdate(this));

  ++current_batch_updates_count_;
  if (current_batch_updates_count_ == 1) {
    FOR_EACH_OBSERVER(ReadingListModelObserver, observers_,
                      ReadingListModelBeganBatchUpdates(this));
  }
  return token;
}

void ReadingListModel::EndBatchUpdates() {
  DCHECK(IsPerformingBatchUpdates());
  --current_batch_updates_count_;
  if (current_batch_updates_count_ == 0) {
    FOR_EACH_OBSERVER(ReadingListModelObserver, observers_,
                      ReadingListModelCompletedBatchUpdates(this));
  }
}
