// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_store.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/protocol/article_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"

using leveldb_proto::ProtoDatabase;
using sync_pb::ArticleSpecifics;
using sync_pb::EntitySpecifics;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncMergeResult;

namespace {
// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.DomDistillerStore. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "DomDistillerStore";
}

namespace dom_distiller {

DomDistillerStore::DomDistillerStore(
    std::unique_ptr<ProtoDatabase<ArticleEntry>> database,
    const base::FilePath& database_dir)
    : database_(std::move(database)),
      database_loaded_(false),
      attachment_store_(syncer::AttachmentStore::CreateInMemoryStore()),
      weak_ptr_factory_(this) {
  database_->Init(kDatabaseUMAClientName, database_dir,
                  leveldb_proto::CreateSimpleOptions(),
                  base::Bind(&DomDistillerStore::OnDatabaseInit,
                             weak_ptr_factory_.GetWeakPtr()));
}

DomDistillerStore::DomDistillerStore(
    std::unique_ptr<ProtoDatabase<ArticleEntry>> database,
    const std::vector<ArticleEntry>& initial_data,
    const base::FilePath& database_dir)
    : database_(std::move(database)),
      database_loaded_(false),
      attachment_store_(syncer::AttachmentStore::CreateInMemoryStore()),
      model_(initial_data),
      weak_ptr_factory_(this) {
  database_->Init(kDatabaseUMAClientName, database_dir,
                  leveldb_proto::CreateSimpleOptions(),
                  base::Bind(&DomDistillerStore::OnDatabaseInit,
                             weak_ptr_factory_.GetWeakPtr()));
}

DomDistillerStore::~DomDistillerStore() {}

// DomDistillerStoreInterface implementation.
syncer::SyncableService* DomDistillerStore::GetSyncableService() {
  return this;
}

bool DomDistillerStore::GetEntryById(const std::string& entry_id,
                                     ArticleEntry* entry) {
  return model_.GetEntryById(entry_id, entry);
}

bool DomDistillerStore::GetEntryByUrl(const GURL& url, ArticleEntry* entry) {
  return model_.GetEntryByUrl(url, entry);
}

void DomDistillerStore::UpdateAttachments(
    const std::string& entry_id,
    std::unique_ptr<ArticleAttachmentsData> attachments_data,
    const UpdateAttachmentsCallback& callback) {
  if (!GetEntryById(entry_id, nullptr)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
  }

  std::unique_ptr<sync_pb::ArticleAttachments> article_attachments(
      new sync_pb::ArticleAttachments());
  syncer::AttachmentList attachment_list;
  attachments_data->CreateSyncAttachments(&attachment_list,
                                          article_attachments.get());

  attachment_store_->Write(
      attachment_list,
      base::Bind(&DomDistillerStore::OnAttachmentsWrite,
                 weak_ptr_factory_.GetWeakPtr(), entry_id,
                 base::Passed(&article_attachments), callback));
}

void DomDistillerStore::OnAttachmentsWrite(
    const std::string& entry_id,
    std::unique_ptr<sync_pb::ArticleAttachments> article_attachments,
    const UpdateAttachmentsCallback& callback,
    const syncer::AttachmentStore::Result& result) {
  bool success = false;
  switch (result) {
    case syncer::AttachmentStore::UNSPECIFIED_ERROR:
    case syncer::AttachmentStore::STORE_INITIALIZATION_FAILED:
      break;
    case syncer::AttachmentStore::SUCCESS:
      success = true;
      break;
  }

  if (success) {
    ArticleEntry entry;
    bool has_entry = GetEntryById(entry_id, &entry);
    if (!has_entry) {
      success = false;
      attachment_store_->Drop(GetAttachmentIds(*article_attachments),
                              syncer::AttachmentStore::DropCallback());
    } else {
      if (entry.has_attachments()) {
        attachment_store_->Drop(GetAttachmentIds(entry.attachments()),
                                syncer::AttachmentStore::DropCallback());
      }
      entry.set_allocated_attachments(article_attachments.release());

      SyncChangeList changes_to_apply;
      changes_to_apply.push_back(SyncChange(
          FROM_HERE, SyncChange::ACTION_UPDATE, CreateLocalData(entry)));

      SyncChangeList changes_applied;
      SyncChangeList changes_missing;

      ApplyChangesToModel(changes_to_apply, &changes_applied, &changes_missing);

      DCHECK_EQ(size_t(0), changes_missing.size());
      DCHECK_EQ(size_t(1), changes_applied.size());

      ApplyChangesToSync(FROM_HERE, changes_applied);
      ApplyChangesToDatabase(changes_applied);
    }
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, success));
}

void DomDistillerStore::GetAttachments(
    const std::string& entry_id,
    const GetAttachmentsCallback& callback) {
  ArticleEntry entry;
  if (!model_.GetEntryById(entry_id, &entry)
      || !entry.has_attachments()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, false, nullptr));
    return;
  }

  // TODO(cjhopman): This should use GetOrDownloadAttachments() once there is a
  // feasible way to use that.
  attachment_store_->Read(GetAttachmentIds(entry.attachments()),
                          base::Bind(&DomDistillerStore::OnAttachmentsRead,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     entry.attachments(), callback));
}

void DomDistillerStore::OnAttachmentsRead(
    const sync_pb::ArticleAttachments& attachments_proto,
    const GetAttachmentsCallback& callback,
    const syncer::AttachmentStore::Result& result,
    std::unique_ptr<syncer::AttachmentMap> attachments,
    std::unique_ptr<syncer::AttachmentIdList> missing) {
  bool success = false;
  switch (result) {
    case syncer::AttachmentStore::UNSPECIFIED_ERROR:
    case syncer::AttachmentStore::STORE_INITIALIZATION_FAILED:
      break;
    case syncer::AttachmentStore::SUCCESS:
      DCHECK(missing->empty());
      success = true;
      break;
  }
  std::unique_ptr<ArticleAttachmentsData> attachments_data;
  if (success) {
    attachments_data = ArticleAttachmentsData::GetFromAttachmentMap(
        attachments_proto, *attachments);
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, success, base::Passed(&attachments_data)));
}

bool DomDistillerStore::AddEntry(const ArticleEntry& entry) {
  return ChangeEntry(entry, SyncChange::ACTION_ADD);
}

bool DomDistillerStore::UpdateEntry(const ArticleEntry& entry) {
  return ChangeEntry(entry, SyncChange::ACTION_UPDATE);
}

bool DomDistillerStore::RemoveEntry(const ArticleEntry& entry) {
  return ChangeEntry(entry, SyncChange::ACTION_DELETE);
}

namespace {

bool VerifyAttachmentsUnchanged(const ArticleEntry& entry,
                                const DomDistillerModel& model) {
    ArticleEntry currentEntry;
    model.GetEntryById(entry.entry_id(), &currentEntry);
    DCHECK_EQ(currentEntry.has_attachments(), entry.has_attachments());
    if (currentEntry.has_attachments()) {
      DCHECK_EQ(currentEntry.attachments().SerializeAsString(),
                entry.attachments().SerializeAsString());
    }
    return true;
}

}  // namespace

bool DomDistillerStore::ChangeEntry(const ArticleEntry& entry,
                                    SyncChange::SyncChangeType changeType) {
  if (!database_loaded_) {
    return false;
  }

  bool hasEntry = model_.GetEntryById(entry.entry_id(), NULL);
  if (hasEntry) {
    if (changeType == SyncChange::ACTION_ADD) {
      DVLOG(1) << "Already have entry with id " << entry.entry_id() << ".";
      return false;
    }
    DCHECK(VerifyAttachmentsUnchanged(entry, model_));
  } else if (changeType != SyncChange::ACTION_ADD) {
    DVLOG(1) << "No entry with id " << entry.entry_id() << " found.";
    return false;
  }

  SyncChangeList changes_to_apply;
  changes_to_apply.push_back(
      SyncChange(FROM_HERE, changeType, CreateLocalData(entry)));

  SyncChangeList changes_applied;
  SyncChangeList changes_missing;

  ApplyChangesToModel(changes_to_apply, &changes_applied, &changes_missing);

  if (changeType == SyncChange::ACTION_UPDATE && changes_applied.size() != 1) {
    DVLOG(1) << "Failed to update entry with id " << entry.entry_id() << ".";
    return false;
  }

  DCHECK_EQ(size_t(0), changes_missing.size());
  DCHECK_EQ(size_t(1), changes_applied.size());

  ApplyChangesToSync(FROM_HERE, changes_applied);
  ApplyChangesToDatabase(changes_applied);

  return true;
}

void DomDistillerStore::AddObserver(DomDistillerObserver* observer) {
  observers_.AddObserver(observer);
}

void DomDistillerStore::RemoveObserver(DomDistillerObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<ArticleEntry> DomDistillerStore::GetEntries() const {
  return model_.GetEntries();
}

// syncer::SyncableService implementation.
SyncMergeResult DomDistillerStore::MergeDataAndStartSyncing(
    ModelType type,
    const SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK_EQ(syncer::ARTICLES, type);
  DCHECK(!sync_processor_);
  DCHECK(!error_factory_);
  sync_processor_ = std::move(sync_processor);
  error_factory_ = std::move(error_handler);

  SyncChangeList database_changes;
  SyncChangeList sync_changes;
  SyncMergeResult result =
      MergeDataWithModel(initial_sync_data, &database_changes, &sync_changes);
  ApplyChangesToDatabase(database_changes);
  ApplyChangesToSync(FROM_HERE, sync_changes);

  return result;
}

void DomDistillerStore::StopSyncing(ModelType type) {
  sync_processor_.reset();
  error_factory_.reset();
}

SyncDataList DomDistillerStore::GetAllSyncData(ModelType type) const {
  return model_.GetAllSyncData();
}

SyncError DomDistillerStore::ProcessSyncChanges(
    const base::Location& from_here,
    const SyncChangeList& change_list) {
  DCHECK(database_loaded_);
  SyncChangeList database_changes;
  SyncChangeList sync_changes;
  ApplyChangesToModel(change_list, &database_changes, &sync_changes);
  ApplyChangesToDatabase(database_changes);
  DCHECK_EQ(size_t(0), sync_changes.size());
  return SyncError();
}

void DomDistillerStore::NotifyObservers(const syncer::SyncChangeList& changes) {
  if (observers_.might_have_observers() && changes.size() > 0) {
    std::vector<DomDistillerObserver::ArticleUpdate> article_changes;
    for (SyncChangeList::const_iterator it = changes.begin();
         it != changes.end(); ++it) {
      DomDistillerObserver::ArticleUpdate article_update;
      switch (it->change_type()) {
        case SyncChange::ACTION_ADD:
          article_update.update_type = DomDistillerObserver::ArticleUpdate::ADD;
          break;
        case SyncChange::ACTION_UPDATE:
          article_update.update_type =
              DomDistillerObserver::ArticleUpdate::UPDATE;
          break;
        case SyncChange::ACTION_DELETE:
          article_update.update_type =
              DomDistillerObserver::ArticleUpdate::REMOVE;
          break;
        case SyncChange::ACTION_INVALID:
          NOTREACHED();
          break;
      }
      const ArticleEntry& entry = GetEntryFromChange(*it);
      article_update.entry_id = entry.entry_id();
      article_changes.push_back(article_update);
    }
    for (DomDistillerObserver& observer : observers_)
      observer.ArticleEntriesUpdated(article_changes);
  }
}

void DomDistillerStore::ApplyChangesToModel(const SyncChangeList& changes,
                                            SyncChangeList* changes_applied,
                                            SyncChangeList* changes_missing) {
  model_.ApplyChangesToModel(changes, changes_applied, changes_missing);
  NotifyObservers(*changes_applied);
}

void DomDistillerStore::OnDatabaseInit(bool success) {
  if (!success) {
    DVLOG(1) << "DOM Distiller database init failed.";
    database_.reset();
    return;
  }
  database_->LoadEntries(base::Bind(&DomDistillerStore::OnDatabaseLoad,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void DomDistillerStore::OnDatabaseLoad(bool success,
                                       std::unique_ptr<EntryVector> entries) {
  if (!success) {
    DVLOG(1) << "DOM Distiller database load failed.";
    database_.reset();
    return;
  }
  database_loaded_ = true;

  SyncDataList data;
  for (EntryVector::iterator it = entries->begin(); it != entries->end();
       ++it) {
    data.push_back(CreateLocalData(*it));
  }
  SyncChangeList changes_applied;
  SyncChangeList database_changes_needed;
  MergeDataWithModel(data, &changes_applied, &database_changes_needed);
  ApplyChangesToDatabase(database_changes_needed);
  ApplyChangesToSync(FROM_HERE, changes_applied);
}

void DomDistillerStore::OnDatabaseSave(bool success) {
  if (!success) {
    DVLOG(1) << "DOM Distiller database save failed."
             << " Disabling modifications and sync.";
    database_.reset();
    database_loaded_ = false;
    StopSyncing(syncer::ARTICLES);
  }
}

bool DomDistillerStore::ApplyChangesToSync(const base::Location& from_here,
                                           const SyncChangeList& change_list) {
  if (!sync_processor_) {
    return false;
  }
  if (change_list.empty()) {
    return true;
  }

  SyncError error = sync_processor_->ProcessSyncChanges(from_here, change_list);
  if (error.IsSet()) {
    StopSyncing(syncer::ARTICLES);
    return false;
  }
  return true;
}

bool DomDistillerStore::ApplyChangesToDatabase(
    const SyncChangeList& change_list) {
  if (!database_loaded_) {
    return false;
  }
  if (change_list.empty()) {
    return true;
  }
  std::unique_ptr<ProtoDatabase<ArticleEntry>::KeyEntryVector> entries_to_save(
      new ProtoDatabase<ArticleEntry>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  for (SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    if (it->change_type() == SyncChange::ACTION_DELETE) {
      ArticleEntry entry = GetEntryFromChange(*it);
      keys_to_remove->push_back(entry.entry_id());
    } else {
      ArticleEntry entry = GetEntryFromChange(*it);
      entries_to_save->push_back(std::make_pair(entry.entry_id(), entry));
    }
  }
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::Bind(&DomDistillerStore::OnDatabaseSave,
                                      weak_ptr_factory_.GetWeakPtr()));
  return true;
}

SyncMergeResult DomDistillerStore::MergeDataWithModel(
    const SyncDataList& data, SyncChangeList* changes_applied,
    SyncChangeList* changes_missing) {
  // TODO(cjhopman): This naive merge algorithm could cause flip-flopping
  // between database/sync of multiple clients.
  DCHECK(changes_applied);
  DCHECK(changes_missing);

  SyncMergeResult result(syncer::ARTICLES);
  result.set_num_items_before_association(model_.GetNumEntries());

  SyncChangeList changes_to_apply;
  model_.CalculateChangesForMerge(data, &changes_to_apply, changes_missing);
  SyncError error;
  ApplyChangesToModel(changes_to_apply, changes_applied, changes_missing);

  int num_added = 0;
  int num_modified = 0;
  for (SyncChangeList::const_iterator it = changes_applied->begin();
       it != changes_applied->end(); ++it) {
    DCHECK(it->IsValid());
    switch (it->change_type()) {
      case SyncChange::ACTION_ADD:
        num_added++;
        break;
      case SyncChange::ACTION_UPDATE:
        num_modified++;
        break;
      default:
        NOTREACHED();
    }
  }
  result.set_num_items_added(num_added);
  result.set_num_items_modified(num_modified);
  result.set_num_items_deleted(0);

  result.set_pre_association_version(0);
  result.set_num_items_after_association(model_.GetNumEntries());
  result.set_error(error);

  return result;
}

}  // namespace dom_distiller
