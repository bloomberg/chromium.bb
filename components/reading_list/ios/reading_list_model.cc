// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/reading_list_model.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"

ReadingListModel::ReadingListModel() : current_batch_updates_count_(0) {}

ReadingListModel::~ReadingListModel() {
  for (auto& observer : observers_) {
    observer.ReadingListModelBeingDeleted(this);
  }
}

// Observer methods.
void ReadingListModel::AddObserver(ReadingListModelObserver* observer) {
  DCHECK(CalledOnValidThread());
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (loaded()) {
    observer->ReadingListModelLoaded(this);
  }
}

void ReadingListModel::RemoveObserver(ReadingListModelObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

// Batch update methods.
bool ReadingListModel::IsPerformingBatchUpdates() const {
  DCHECK(CalledOnValidThread());
  return current_batch_updates_count_ > 0;
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
ReadingListModel::CreateBatchToken() {
  return base::MakeUnique<ReadingListModel::ScopedReadingListBatchUpdate>(this);
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
ReadingListModel::BeginBatchUpdates() {
  DCHECK(CalledOnValidThread());
  auto token = CreateBatchToken();

  ++current_batch_updates_count_;
  if (current_batch_updates_count_ == 1) {
    EnteringBatchUpdates();
  }
  return token;
}

void ReadingListModel::EnteringBatchUpdates() {
  DCHECK(CalledOnValidThread());
  for (auto& observer : observers_)
    observer.ReadingListModelBeganBatchUpdates(this);
}

void ReadingListModel::EndBatchUpdates() {
  DCHECK(CalledOnValidThread());
  DCHECK(IsPerformingBatchUpdates());
  DCHECK(current_batch_updates_count_ > 0);
  --current_batch_updates_count_;
  if (current_batch_updates_count_ == 0) {
    LeavingBatchUpdates();
  }
}

void ReadingListModel::LeavingBatchUpdates() {
  DCHECK(CalledOnValidThread());
  for (auto& observer : observers_)
    observer.ReadingListModelCompletedBatchUpdates(this);
}

ReadingListModel::ScopedReadingListBatchUpdate::
    ~ScopedReadingListBatchUpdate() {
  model_->EndBatchUpdates();
}
