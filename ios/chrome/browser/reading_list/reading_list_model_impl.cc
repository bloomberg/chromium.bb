// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_model_impl.h"

#include "ios/chrome/browser/reading_list/reading_list_model_storage.h"
#include "url/gurl.h"

ReadingListModelImpl::ReadingListModelImpl() : ReadingListModelImpl(NULL) {}

ReadingListModelImpl::ReadingListModelImpl(
    std::unique_ptr<ReadingListModelStorage> storage)
    : hasUnseen_(false) {
  if (storage) {
    storageLayer_ = std::move(storage);
    read_ = storageLayer_->LoadPersistentReadList();
    unread_ = storageLayer_->LoadPersistentUnreadList();
    hasUnseen_ = storageLayer_->LoadPersistentHasUnseen();
  }
  loaded_ = true;
}
ReadingListModelImpl::~ReadingListModelImpl() {}

void ReadingListModelImpl::Shutdown() {
  for (auto& observer : observers_)
    observer.ReadingListModelBeingDeleted(this);
  loaded_ = false;
}

bool ReadingListModelImpl::loaded() const {
  return loaded_;
}

size_t ReadingListModelImpl::unread_size() const {
  DCHECK(loaded());
  return unread_.size();
}

size_t ReadingListModelImpl::read_size() const {
  DCHECK(loaded());
  return read_.size();
}

bool ReadingListModelImpl::HasUnseenEntries() const {
  DCHECK(loaded());
  return unread_size() && hasUnseen_;
}

void ReadingListModelImpl::ResetUnseenEntries() {
  DCHECK(loaded());
  hasUnseen_ = false;
  if (storageLayer_ && !IsPerformingBatchUpdates())
    storageLayer_->SavePersistentHasUnseen(false);
}

const ReadingListEntry& ReadingListModelImpl::GetUnreadEntryAtIndex(
    size_t index) const {
  DCHECK(loaded());
  return unread_[index];
}

const ReadingListEntry& ReadingListModelImpl::GetReadEntryAtIndex(
    size_t index) const {
  DCHECK(loaded());
  return read_[index];
}

const ReadingListEntry* ReadingListModelImpl::GetEntryFromURL(
    const GURL& gurl) const {
  DCHECK(loaded());
  ReadingListEntry entry(gurl, std::string());
  auto it = std::find(read_.begin(), read_.end(), entry);
  if (it == read_.end()) {
    it = std::find(unread_.begin(), unread_.end(), entry);
    if (it == unread_.end())
      return nullptr;
  }
  return &(*it);
}

bool ReadingListModelImpl::CallbackEntryURL(
    const GURL& url,
    base::Callback<void(const ReadingListEntry&)> callback) const {
  DCHECK(loaded());
  const ReadingListEntry* entry = GetEntryFromURL(url);
  if (entry) {
    callback.Run(*entry);
    return true;
  }
  return false;
}

void ReadingListModelImpl::RemoveEntryByUrl(const GURL& url) {
  DCHECK(loaded());
  const ReadingListEntry entry(url, std::string());

  auto result = std::find(unread_.begin(), unread_.end(), entry);
  if (result != unread_.end()) {
    for (auto& observer : observers_) {
      observer.ReadingListWillRemoveUnreadEntry(
          this, std::distance(unread_.begin(), result));
    }
    unread_.erase(result);
    if (storageLayer_ && !IsPerformingBatchUpdates())
      storageLayer_->SavePersistentUnreadList(unread_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }

  result = std::find(read_.begin(), read_.end(), entry);
  if (result != read_.end()) {
    for (auto& observer : observers_) {
      observer.ReadingListWillRemoveReadEntry(
          this, std::distance(read_.begin(), result));
    }
    read_.erase(result);
    if (storageLayer_ && !IsPerformingBatchUpdates())
      storageLayer_->SavePersistentReadList(read_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
}

const ReadingListEntry& ReadingListModelImpl::AddEntry(
    const GURL& url,
    const std::string& title) {
  DCHECK(loaded());
  RemoveEntryByUrl(url);
  ReadingListEntry entry(url, title);
  for (auto& observer : observers_)
    observer.ReadingListWillAddUnreadEntry(this, entry);
  unread_.insert(unread_.begin(), std::move(entry));
  hasUnseen_ = true;
  if (storageLayer_ && !IsPerformingBatchUpdates()) {
    storageLayer_->SavePersistentUnreadList(unread_);
    storageLayer_->SavePersistentHasUnseen(true);
  }
  for (auto& observer : observers_)
    observer.ReadingListDidApplyChanges(this);
  return *unread_.begin();
}

void ReadingListModelImpl::MarkUnreadByURL(const GURL& url) {
  DCHECK(loaded());
  ReadingListEntry entry(url, std::string());
  auto result = std::find(read_.begin(), read_.end(), entry);
  if (result == read_.end())
    return;

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillMoveEntry(this,
                                      std::distance(read_.begin(), result));
  }

  unread_.insert(unread_.begin(), std::move(*result));
  read_.erase(result);

  if (storageLayer_ && !IsPerformingBatchUpdates()) {
    storageLayer_->SavePersistentUnreadList(read_);
    storageLayer_->SavePersistentReadList(unread_);
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::MarkReadByURL(const GURL& url) {
  DCHECK(loaded());
  ReadingListEntry entry(url, std::string());
  auto result = std::find(unread_.begin(), unread_.end(), entry);
  if (result == unread_.end())
    return;

  for (auto& observer : observers_) {
    observer.ReadingListWillMoveEntry(this,
                                      std::distance(unread_.begin(), result));
  }

  read_.insert(read_.begin(), std::move(*result));
  unread_.erase(result);

  if (storageLayer_ && !IsPerformingBatchUpdates()) {
    storageLayer_->SavePersistentUnreadList(unread_);
    storageLayer_->SavePersistentReadList(read_);
  }
  for (auto& observer : observers_)
    observer.ReadingListDidApplyChanges(this);
}

void ReadingListModelImpl::SetEntryTitle(const GURL& url,
                                         const std::string& title) {
  DCHECK(loaded());
  const ReadingListEntry entry(url, std::string());

  auto result = std::find(unread_.begin(), unread_.end(), entry);
  if (result != unread_.end()) {
    for (auto& observer : observers_) {
      observer.ReadingListWillUpdateUnreadEntry(
          this, std::distance(unread_.begin(), result));
    }
    result->SetTitle(title);
    if (storageLayer_ && !IsPerformingBatchUpdates())
      storageLayer_->SavePersistentUnreadList(unread_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }

  result = std::find(read_.begin(), read_.end(), entry);
  if (result != read_.end()) {
    for (auto& observer : observers_) {
      observer.ReadingListWillUpdateReadEntry(
          this, std::distance(read_.begin(), result));
    }
    result->SetTitle(title);
    if (storageLayer_ && !IsPerformingBatchUpdates())
      storageLayer_->SavePersistentReadList(read_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
}

void ReadingListModelImpl::SetEntryDistilledURL(const GURL& url,
                                                const GURL& distilled_url) {
  DCHECK(loaded());
  const ReadingListEntry entry(url, std::string());

  auto result = std::find(unread_.begin(), unread_.end(), entry);
  if (result != unread_.end()) {
    for (auto& observer : observers_) {
      observer.ReadingListWillUpdateUnreadEntry(
          this, std::distance(unread_.begin(), result));
    }
    result->SetDistilledURL(distilled_url);
    if (storageLayer_ && !IsPerformingBatchUpdates())
      storageLayer_->SavePersistentUnreadList(unread_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }

  result = std::find(read_.begin(), read_.end(), entry);
  if (result != read_.end()) {
    for (auto& observer : observers_) {
      observer.ReadingListWillUpdateReadEntry(
          this, std::distance(read_.begin(), result));
    }
    result->SetDistilledURL(distilled_url);
    if (storageLayer_ && !IsPerformingBatchUpdates())
      storageLayer_->SavePersistentReadList(read_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
}

void ReadingListModelImpl::SetEntryDistilledState(
    const GURL& url,
    ReadingListEntry::DistillationState state) {
  DCHECK(loaded());
  const ReadingListEntry entry(url, std::string());

  auto result = std::find(unread_.begin(), unread_.end(), entry);
  if (result != unread_.end()) {
    for (auto& observer : observers_) {
      observer.ReadingListWillUpdateUnreadEntry(
          this, std::distance(unread_.begin(), result));
    }
    result->SetDistilledState(state);
    if (storageLayer_ && !IsPerformingBatchUpdates())
      storageLayer_->SavePersistentUnreadList(unread_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }

  result = std::find(read_.begin(), read_.end(), entry);
  if (result != read_.end()) {
    for (auto& observer : observers_) {
      observer.ReadingListWillUpdateReadEntry(
          this, std::distance(read_.begin(), result));
    }
    result->SetDistilledState(state);
    if (storageLayer_ && !IsPerformingBatchUpdates())
      storageLayer_->SavePersistentReadList(read_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
};

void ReadingListModelImpl::EndBatchUpdates() {
  ReadingListModel::EndBatchUpdates();
  if (IsPerformingBatchUpdates() || !storageLayer_) {
    return;
  }
  storageLayer_->SavePersistentUnreadList(unread_);
  storageLayer_->SavePersistentReadList(read_);
  storageLayer_->SavePersistentHasUnseen(hasUnseen_);
}
