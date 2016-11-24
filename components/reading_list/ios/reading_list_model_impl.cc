// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/reading_list_model_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/ios/reading_list_model_storage.h"
#include "components/reading_list/ios/reading_list_pref_names.h"
#include "url/gurl.h"

ReadingListModelImpl::ReadingListModelImpl()
    : ReadingListModelImpl(nullptr, nullptr) {}

ReadingListModelImpl::ReadingListModelImpl(
    std::unique_ptr<ReadingListModelStorage> storage,
    PrefService* pref_service)
    : pref_service_(pref_service),
      has_unseen_(false),
      loaded_(false),
      weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  if (storage) {
    storage_layer_ = std::move(storage);
    storage_layer_->SetReadingListModel(this, this);
  } else {
    loaded_ = true;
    read_ = base::MakeUnique<ReadingListEntries>();
    unread_ = base::MakeUnique<ReadingListEntries>();
  }
  has_unseen_ = GetPersistentHasUnseen();
}

ReadingListModelImpl::~ReadingListModelImpl() {}

void ReadingListModelImpl::StoreLoaded(
    std::unique_ptr<ReadingListEntries> unread,
    std::unique_ptr<ReadingListEntries> read) {
  DCHECK(CalledOnValidThread());
  read_ = std::move(read);
  unread_ = std::move(unread);
  loaded_ = true;
  SortEntries();
  for (auto& observer : observers_)
    observer.ReadingListModelLoaded(this);
}

void ReadingListModelImpl::Shutdown() {
  DCHECK(CalledOnValidThread());
  for (auto& observer : observers_)
    observer.ReadingListModelBeingDeleted(this);
  loaded_ = false;
}

bool ReadingListModelImpl::loaded() const {
  DCHECK(CalledOnValidThread());
  return loaded_;
}

size_t ReadingListModelImpl::unread_size() const {
  DCHECK(CalledOnValidThread());
  if (!loaded())
    return 0;
  return unread_->size();
}

size_t ReadingListModelImpl::read_size() const {
  DCHECK(CalledOnValidThread());
  if (!loaded())
    return 0;
  return read_->size();
}

bool ReadingListModelImpl::HasUnseenEntries() const {
  DCHECK(CalledOnValidThread());
  if (!loaded())
    return false;
  return unread_size() && has_unseen_;
}

void ReadingListModelImpl::ResetUnseenEntries() {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  has_unseen_ = false;
  if (!IsPerformingBatchUpdates())
    SetPersistentHasUnseen(false);
}

const ReadingListEntry& ReadingListModelImpl::GetUnreadEntryAtIndex(
    size_t index) const {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  return unread_->at(index);
}

const ReadingListEntry& ReadingListModelImpl::GetReadEntryAtIndex(
    size_t index) const {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  return read_->at(index);
}

const ReadingListEntry* ReadingListModelImpl::GetEntryFromURL(
    const GURL& gurl,
    bool* read) const {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  return GetMutableEntryFromURL(gurl, read);
}

ReadingListEntry* ReadingListModelImpl::GetMutableEntryFromURL(
    const GURL& gurl,
    bool* read) const {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  bool is_read;
  ReadingListEntry entry(gurl, std::string());
  auto it = std::find(read_->begin(), read_->end(), entry);
  is_read = true;
  if (it == read_->end()) {
    it = std::find(unread_->begin(), unread_->end(), entry);
    is_read = false;
    if (it == unread_->end())
      return nullptr;
  }
  if (read) {
    *read = is_read;
  }
  return &(*it);
}

bool ReadingListModelImpl::CallbackEntryURL(
    const GURL& url,
    base::Callback<void(const ReadingListEntry&)> callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  const ReadingListEntry* entry = GetMutableEntryFromURL(url, nullptr);
  if (entry) {
    callback.Run(*entry);
    return true;
  }
  return false;
}

void ReadingListModelImpl::MoveEntryFrom(ReadingListEntries* entries,
                                         const ReadingListEntry& entry,
                                         bool read) {
  auto result = std::find(entries->begin(), entries->end(), entry);
  DCHECK(result != entries->end());
  int index = std::distance(entries->begin(), result);
  for (auto& observer : observers_)
    observer.ReadingListWillMoveEntry(this, index, read);
  entries->erase(result);
}

void ReadingListModelImpl::SyncAddEntry(std::unique_ptr<ReadingListEntry> entry,
                                        bool read) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  // entry must not already exist.
  DCHECK(GetMutableEntryFromURL(entry->URL(), nullptr) == nullptr);
  if (read) {
    for (auto& observer : observers_)
      observer.ReadingListWillAddReadEntry(this, *entry);
    read_->insert(read_->begin(), std::move(*entry));
  } else {
    for (auto& observer : observers_)
      observer.ReadingListWillAddUnreadEntry(this, *entry);
    has_unseen_ = true;
    SetPersistentHasUnseen(true);
    unread_->insert(unread_->begin(), std::move(*entry));
  }
  for (auto& observer : observers_)
    observer.ReadingListDidApplyChanges(this);
}

ReadingListEntry* ReadingListModelImpl::SyncMergeEntry(
    std::unique_ptr<ReadingListEntry> entry,
    bool read) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  bool is_existing_entry_read;
  ReadingListEntry* existing_entry =
      GetMutableEntryFromURL(entry->URL(), &is_existing_entry_read);

  DCHECK(existing_entry);
  DCHECK(existing_entry->UpdateTime() < entry->UpdateTime());

  // Merge local data in new entry.
  entry->MergeLocalStateFrom(*existing_entry);

  if (is_existing_entry_read) {
    MoveEntryFrom(read_.get(), *existing_entry, true);
  } else {
    MoveEntryFrom(unread_.get(), *existing_entry, false);
  }

  ReadingListEntries::iterator added_iterator;
  if (read) {
    read_->push_back(std::move(*entry));
    added_iterator = read_->end() - 1;
  } else {
    unread_->push_back(std::move(*entry));
    added_iterator = unread_->end() - 1;
  }
  for (auto& observer : observers_)
    observer.ReadingListDidApplyChanges(this);

  ReadingListEntry& merged_entry = *added_iterator;
  return &merged_entry;
}

void ReadingListModelImpl::SyncRemoveEntry(const GURL& url) {
  RemoveEntryByURLImpl(url, true);
}

void ReadingListModelImpl::RemoveEntryByURL(const GURL& url) {
  RemoveEntryByURLImpl(url, false);
}

void ReadingListModelImpl::RemoveEntryByURLImpl(const GURL& url,
                                                bool from_sync) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  const ReadingListEntry entry(url, std::string());

  auto result = std::find(unread_->begin(), unread_->end(), entry);
  if (result != unread_->end()) {
    for (auto& observer : observers_)
      observer.ReadingListWillRemoveUnreadEntry(
          this, std::distance(unread_->begin(), result));

    if (storage_layer_ && !from_sync) {
      storage_layer_->RemoveEntry(*result);
    }
    unread_->erase(result);

    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }

  result = std::find(read_->begin(), read_->end(), entry);
  if (result != read_->end()) {
    for (auto& observer : observers_)
      observer.ReadingListWillRemoveReadEntry(
          this, std::distance(read_->begin(), result));
    if (storage_layer_ && !from_sync) {
      storage_layer_->RemoveEntry(*result);
    }
    read_->erase(result);

    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
}

const ReadingListEntry& ReadingListModelImpl::AddEntry(
    const GURL& url,
    const std::string& title) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  RemoveEntryByURL(url);

  std::string trimmedTitle(title);
  base::TrimWhitespaceASCII(trimmedTitle, base::TRIM_ALL, &trimmedTitle);

  ReadingListEntry entry(url, trimmedTitle);
  for (auto& observer : observers_)
    observer.ReadingListWillAddUnreadEntry(this, entry);
  has_unseen_ = true;
  SetPersistentHasUnseen(true);
  if (storage_layer_) {
    storage_layer_->SaveEntry(entry, false);
  }
  unread_->insert(unread_->begin(), std::move(entry));

  for (auto& observer : observers_)
    observer.ReadingListDidApplyChanges(this);
  return *unread_->begin();
}

void ReadingListModelImpl::MarkUnreadByURL(const GURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  ReadingListEntry entry(url, std::string());
  auto result = std::find(read_->begin(), read_->end(), entry);
  if (result == read_->end())
    return;

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillMoveEntry(
        this, std::distance(read_->begin(), result), true);
  }

  result->MarkEntryUpdated();
  if (storage_layer_) {
    storage_layer_->SaveEntry(*result, false);
  }

  unread_->insert(unread_->begin(), std::move(*result));
  read_->erase(result);

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::MarkReadByURL(const GURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  ReadingListEntry entry(url, std::string());
  auto result = std::find(unread_->begin(), unread_->end(), entry);
  if (result == unread_->end())
    return;

  for (auto& observer : observers_)
    observer.ReadingListWillMoveEntry(
        this, std::distance(unread_->begin(), result), false);

  result->MarkEntryUpdated();
  if (storage_layer_) {
    storage_layer_->SaveEntry(*result, true);
  }

  read_->insert(read_->begin(), std::move(*result));
  unread_->erase(result);

  for (auto& observer : observers_)
    observer.ReadingListDidApplyChanges(this);
}

void ReadingListModelImpl::SetEntryTitle(const GURL& url,
                                         const std::string& title) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  const ReadingListEntry entry(url, std::string());

  auto result = std::find(unread_->begin(), unread_->end(), entry);
  if (result != unread_->end()) {
    for (auto& observer : observers_)
      observer.ReadingListWillUpdateUnreadEntry(
          this, std::distance(unread_->begin(), result));
    result->SetTitle(title);

    if (storage_layer_) {
      storage_layer_->SaveEntry(*result, false);
    }
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }

  result = std::find(read_->begin(), read_->end(), entry);
  if (result != read_->end()) {
    for (auto& observer : observers_)
      observer.ReadingListWillUpdateReadEntry(
          this, std::distance(read_->begin(), result));
    result->SetTitle(title);
    if (storage_layer_) {
      storage_layer_->SaveEntry(*result, true);
    }
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
}

void ReadingListModelImpl::SetEntryDistilledPath(
    const GURL& url,
    const base::FilePath& distilled_path) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  const ReadingListEntry entry(url, std::string());

  auto result = std::find(unread_->begin(), unread_->end(), entry);
  if (result != unread_->end()) {
    for (auto& observer : observers_)
      observer.ReadingListWillUpdateUnreadEntry(
          this, std::distance(unread_->begin(), result));
    result->SetDistilledPath(distilled_path);
    if (storage_layer_) {
      storage_layer_->SaveEntry(*result, false);
    }
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }

  result = std::find(read_->begin(), read_->end(), entry);
  if (result != read_->end()) {
    for (auto& observer : observers_)
      observer.ReadingListWillUpdateReadEntry(
          this, std::distance(read_->begin(), result));
    result->SetDistilledPath(distilled_path);
    if (storage_layer_) {
      storage_layer_->SaveEntry(*result, true);
    }
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
}

void ReadingListModelImpl::SetEntryDistilledState(
    const GURL& url,
    ReadingListEntry::DistillationState state) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  const ReadingListEntry entry(url, std::string());

  auto result = std::find(unread_->begin(), unread_->end(), entry);
  if (result != unread_->end()) {
    for (auto& observer : observers_)
      observer.ReadingListWillUpdateUnreadEntry(
          this, std::distance(unread_->begin(), result));
    result->SetDistilledState(state);
    if (storage_layer_) {
      storage_layer_->SaveEntry(*result, false);
    }
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }

  result = std::find(read_->begin(), read_->end(), entry);
  if (result != read_->end()) {
    for (auto& observer : observers_)
      observer.ReadingListWillUpdateReadEntry(
          this, std::distance(read_->begin(), result));
    result->SetDistilledState(state);
    if (storage_layer_) {
      storage_layer_->SaveEntry(*result, true);
    }
    for (auto& observer : observers_)
      observer.ReadingListDidApplyChanges(this);
    return;
  }
};

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
ReadingListModelImpl::CreateBatchToken() {
  return base::MakeUnique<ReadingListModelImpl::ScopedReadingListBatchUpdate>(
      this);
}

ReadingListModelImpl::ScopedReadingListBatchUpdate::
    ScopedReadingListBatchUpdate(ReadingListModelImpl* model)
    : ReadingListModel::ScopedReadingListBatchUpdate::
          ScopedReadingListBatchUpdate(model) {
  if (model->StorageLayer()) {
    storage_token_ = model->StorageLayer()->EnsureBatchCreated();
  }
}

ReadingListModelImpl::ScopedReadingListBatchUpdate::
    ~ScopedReadingListBatchUpdate() {
  storage_token_.reset();
}

void ReadingListModelImpl::LeavingBatchUpdates() {
  DCHECK(CalledOnValidThread());
  if (storage_layer_) {
    SetPersistentHasUnseen(has_unseen_);
    SortEntries();
  }
  ReadingListModel::LeavingBatchUpdates();
}

void ReadingListModelImpl::EnteringBatchUpdates() {
  DCHECK(CalledOnValidThread());
  ReadingListModel::EnteringBatchUpdates();
}

void ReadingListModelImpl::SetPersistentHasUnseen(bool has_unseen) {
  DCHECK(CalledOnValidThread());
  if (!pref_service_) {
    return;
  }
  pref_service_->SetBoolean(reading_list::prefs::kReadingListHasUnseenEntries,
                            has_unseen);
}

bool ReadingListModelImpl::GetPersistentHasUnseen() {
  DCHECK(CalledOnValidThread());
  if (!pref_service_) {
    return false;
  }
  return pref_service_->GetBoolean(
      reading_list::prefs::kReadingListHasUnseenEntries);
}

syncer::ModelTypeSyncBridge* ReadingListModelImpl::GetModelTypeSyncBridge() {
  DCHECK(loaded());
  if (!storage_layer_)
    return nullptr;
  return storage_layer_->GetModelTypeSyncBridge();
}

void ReadingListModelImpl::SortEntries() {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  std::sort(read_->begin(), read_->end(),
            ReadingListEntry::CompareEntryUpdateTime);
  std::sort(unread_->begin(), unread_->end(),
            ReadingListEntry::CompareEntryUpdateTime);
}

ReadingListModelStorage* ReadingListModelImpl::StorageLayer() {
  return storage_layer_.get();
}
