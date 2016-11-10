// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_model_impl.h"

#include "base/strings/string_util.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/reading_list/reading_list_model_storage.h"
#include "ios/chrome/browser/reading_list/reading_list_pref_names.h"
#include "url/gurl.h"

ReadingListModelImpl::ReadingListModelImpl()
    : ReadingListModelImpl(nullptr, nullptr) {}

ReadingListModelImpl::ReadingListModelImpl(
    std::unique_ptr<ReadingListModelStorage> storage,
    PrefService* pref_service)
    : pref_service_(pref_service), has_unseen_(false) {
  if (storage) {
    storage_layer_ = std::move(storage);
    read_ = storage_layer_->LoadPersistentReadList();
    unread_ = storage_layer_->LoadPersistentUnreadList();
    has_unseen_ = GetPersistentHasUnseen();
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
  return unread_size() && has_unseen_;
}

void ReadingListModelImpl::ResetUnseenEntries() {
  DCHECK(loaded());
  has_unseen_ = false;
  if (storage_layer_ && !IsPerformingBatchUpdates())
    SetPersistentHasUnseen(false);
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

void ReadingListModelImpl::RemoveEntryByURL(const GURL& url) {
  DCHECK(loaded());
  const ReadingListEntry entry(url, std::string());

  auto result = std::find(unread_.begin(), unread_.end(), entry);
  if (result != unread_.end()) {
    for (auto& observer : observers_) {
      observer.ReadingListWillRemoveUnreadEntry(
          this, std::distance(unread_.begin(), result));
    }
    unread_.erase(result);
    if (storage_layer_ && !IsPerformingBatchUpdates())
      storage_layer_->SavePersistentUnreadList(unread_);
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
    if (storage_layer_ && !IsPerformingBatchUpdates())
      storage_layer_->SavePersistentReadList(read_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
}

const ReadingListEntry& ReadingListModelImpl::AddEntry(
    const GURL& url,
    const std::string& title) {
  DCHECK(loaded());
  RemoveEntryByURL(url);

  std::string trimmedTitle(title);
  base::TrimWhitespaceASCII(trimmedTitle, base::TRIM_ALL, &trimmedTitle);

  ReadingListEntry entry(url, trimmedTitle);
  for (auto& observer : observers_)
    observer.ReadingListWillAddUnreadEntry(this, entry);
  unread_.insert(unread_.begin(), std::move(entry));
  has_unseen_ = true;
  if (storage_layer_ && !IsPerformingBatchUpdates()) {
    storage_layer_->SavePersistentUnreadList(unread_);
    SetPersistentHasUnseen(true);
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

  if (storage_layer_ && !IsPerformingBatchUpdates()) {
    storage_layer_->SavePersistentUnreadList(read_);
    storage_layer_->SavePersistentReadList(unread_);
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

  if (storage_layer_ && !IsPerformingBatchUpdates()) {
    storage_layer_->SavePersistentUnreadList(unread_);
    storage_layer_->SavePersistentReadList(read_);
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
    if (storage_layer_ && !IsPerformingBatchUpdates())
      storage_layer_->SavePersistentUnreadList(unread_);
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
    if (storage_layer_ && !IsPerformingBatchUpdates())
      storage_layer_->SavePersistentReadList(read_);
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
    if (storage_layer_ && !IsPerformingBatchUpdates())
      storage_layer_->SavePersistentUnreadList(unread_);
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
    if (storage_layer_ && !IsPerformingBatchUpdates())
      storage_layer_->SavePersistentReadList(read_);
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
    if (storage_layer_ && !IsPerformingBatchUpdates())
      storage_layer_->SavePersistentUnreadList(unread_);
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
    if (storage_layer_ && !IsPerformingBatchUpdates())
      storage_layer_->SavePersistentReadList(read_);
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
};

void ReadingListModelImpl::EndBatchUpdates() {
  ReadingListModel::EndBatchUpdates();
  if (IsPerformingBatchUpdates() || !storage_layer_) {
    return;
  }
  storage_layer_->SavePersistentUnreadList(unread_);
  storage_layer_->SavePersistentReadList(read_);
  SetPersistentHasUnseen(has_unseen_);
}

void ReadingListModelImpl::SetPersistentHasUnseen(bool has_unseen) {
  if (!pref_service_) {
    return;
  }
  pref_service_->SetBoolean(reading_list::prefs::kReadingListHasUnseenEntries,
                            has_unseen);
}

bool ReadingListModelImpl::GetPersistentHasUnseen() {
  if (!pref_service_) {
    return false;
  }
  return pref_service_->GetBoolean(
      reading_list::prefs::kReadingListHasUnseenEntries);
}
