// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_metadata_store_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/proto/offline_pages.pb.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "url/gurl.h"

using leveldb_proto::ProtoDatabase;

namespace {
// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.OfflinePageMetadataStore. Changing this
// needs to synchronize with histograms.xml, AND will also become incompatible
// with older browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "OfflinePageMetadataStore";
}

namespace offline_pages {
namespace {

void OfflinePageItemToEntry(const OfflinePageItem& item,
                            offline_pages::OfflinePageEntry* item_proto) {
  DCHECK(item_proto);
  item_proto->set_url(item.url.spec());
  item_proto->set_offline_id(item.offline_id);
  item_proto->set_version(item.version);
  std::string path_string;
#if defined(OS_POSIX)
  path_string = item.file_path.value();
#elif defined(OS_WIN)
  path_string = base::WideToUTF8(item.file_path.value());
#endif
  item_proto->set_file_path(path_string);
  item_proto->set_file_size(item.file_size);
  item_proto->set_creation_time(item.creation_time.ToInternalValue());
  item_proto->set_last_access_time(item.last_access_time.ToInternalValue());
  item_proto->set_access_count(item.access_count);
  item_proto->set_flags(
      static_cast<::offline_pages::OfflinePageEntry_Flags>(item.flags));
  item_proto->set_client_id_name_space(item.client_id.name_space);
  item_proto->set_client_id(item.client_id.id);
}

bool OfflinePageItemFromEntry(const offline_pages::OfflinePageEntry& item_proto,
                              OfflinePageItem* item) {
  DCHECK(item);
  bool has_offline_id =
      item_proto.has_offline_id() || item_proto.has_deprecated_bookmark_id();
  if (!item_proto.has_url() || !has_offline_id || !item_proto.has_version() ||
      !item_proto.has_file_path()) {
    return false;
  }
  item->url = GURL(item_proto.url());
  item->offline_id = item_proto.offline_id();
  item->version = item_proto.version();
#if defined(OS_POSIX)
  item->file_path = base::FilePath(item_proto.file_path());
#elif defined(OS_WIN)
  item->file_path = base::FilePath(base::UTF8ToWide(item_proto.file_path()));
#endif
  if (item_proto.has_file_size()) {
    item->file_size = item_proto.file_size();
  }
  if (item_proto.has_creation_time()) {
    item->creation_time =
        base::Time::FromInternalValue(item_proto.creation_time());
  }
  if (item_proto.has_last_access_time()) {
    item->last_access_time =
        base::Time::FromInternalValue(item_proto.last_access_time());
  }
  if (item_proto.has_access_count()) {
    item->access_count = item_proto.access_count();
  }
  if (item_proto.has_flags()) {
    item->flags = static_cast<OfflinePageItem::Flags>(item_proto.flags());
  }
  item->client_id.name_space = item_proto.client_id_name_space();
  item->client_id.id = item_proto.client_id();

  return true;
}

}  // namespace

OfflinePageMetadataStoreImpl::OfflinePageMetadataStoreImpl(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& database_dir)
    : background_task_runner_(background_task_runner),
      database_dir_(database_dir),
      weak_ptr_factory_(this) {
}

OfflinePageMetadataStoreImpl::~OfflinePageMetadataStoreImpl() {
}

void OfflinePageMetadataStoreImpl::Load(const LoadCallback& callback) {
  // First initialize the database.
  database_.reset(new leveldb_proto::ProtoDatabaseImpl<OfflinePageEntry>(
      background_task_runner_));
  database_->Init(kDatabaseUMAClientName, database_dir_,
                  base::Bind(&OfflinePageMetadataStoreImpl::LoadContinuation,
                             weak_ptr_factory_.GetWeakPtr(),
                             callback));
}

void OfflinePageMetadataStoreImpl::LoadContinuation(
    const LoadCallback& callback,
    bool success) {
  if (!success) {
    NotifyLoadResult(callback,
                     STORE_INIT_FAILED,
                     std::vector<OfflinePageItem>());
    return;
  }

  // After initialization, start to load the data.
  database_->LoadEntries(
      base::Bind(&OfflinePageMetadataStoreImpl::LoadDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void OfflinePageMetadataStoreImpl::LoadDone(
    const LoadCallback& callback,
    bool success,
    std::unique_ptr<std::vector<OfflinePageEntry>> entries) {
  DCHECK(entries);

  std::vector<OfflinePageItem> result;
  std::unique_ptr<ProtoDatabase<OfflinePageEntry>::KeyEntryVector>
      entries_to_update(new ProtoDatabase<OfflinePageEntry>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  LoadStatus status = LOAD_SUCCEEDED;

  if (success) {
    for (auto& entry : *entries) {
      OfflinePageItem item;
      // We don't want to fail the entire database if one item is corrupt,
      // so log error and keep going.
      if (!OfflinePageItemFromEntry(entry, &item)) {
        LOG(ERROR) << "failed to parse entry: " << entry.url() << " skipping.";
        continue;
      }
      // Legacy storage.  We upgrade them to the new offline_id keyed storage.
      // TODO(bburns): Remove this eventually when we are sure everyone is
      // upgraded.
      if (!entry.has_offline_id()) {
        item.offline_id = OfflinePageModel::GenerateOfflineId();

        if (!entry.has_deprecated_bookmark_id()) {
          LOG(ERROR) << "unexpected entry missing bookmark id";
          continue;
        }
        item.client_id.name_space = offline_pages::kBookmarkNamespace;
        item.client_id.id = base::Int64ToString(entry.deprecated_bookmark_id());

        OfflinePageEntry upgraded_entry;
        OfflinePageItemToEntry(item, &upgraded_entry);
        entries_to_update->push_back(
            std::make_pair(base::Int64ToString(upgraded_entry.offline_id()),
                           upgraded_entry));
        // Remove the old entry that is indexed with deprecated id.
        keys_to_remove->push_back(item.client_id.id);
      }
      result.push_back(item);
    }
  } else {
    status = STORE_LOAD_FAILED;
  }

  // If we couldn't load _anything_ report a parse failure.
  if (entries->size() > 0 && result.size() == 0) {
    status = DATA_PARSING_FAILED;
  }

  if (status == LOAD_SUCCEEDED && entries_to_update->size() > 0) {
    UpdateEntries(std::move(entries_to_update), std::move(keys_to_remove),
                  base::Bind(&OfflinePageMetadataStoreImpl::DatabaseUpdateDone,
                             weak_ptr_factory_.GetWeakPtr(), callback, status,
                             std::move(result)));
  } else {
    NotifyLoadResult(callback, status, result);
  }
}

void OfflinePageMetadataStoreImpl::NotifyLoadResult(
    const LoadCallback& callback,
    LoadStatus status,
    const std::vector<OfflinePageItem>& result) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.LoadStatus",
                            status,
                            OfflinePageMetadataStore::LOAD_STATUS_COUNT);
  if (status == LOAD_SUCCEEDED) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.SavedPageCount", result.size());
  } else {
    DVLOG(1) << "Offline pages database loading failed: " << status;
    database_.reset();
  }
  callback.Run(status, result);
}

void OfflinePageMetadataStoreImpl::AddOrUpdateOfflinePage(
    const OfflinePageItem& offline_page_item,
    const UpdateCallback& callback) {
  std::unique_ptr<ProtoDatabase<OfflinePageEntry>::KeyEntryVector>
      entries_to_save(new ProtoDatabase<OfflinePageEntry>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  OfflinePageEntry offline_page_proto;
  OfflinePageItemToEntry(offline_page_item, &offline_page_proto);

  entries_to_save->push_back(std::make_pair(
      base::Int64ToString(offline_page_item.offline_id), offline_page_proto));

  UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                callback);
}

void OfflinePageMetadataStoreImpl::RemoveOfflinePages(
    const std::vector<int64_t>& offline_ids,
    const UpdateCallback& callback) {
  std::unique_ptr<ProtoDatabase<OfflinePageEntry>::KeyEntryVector>
      entries_to_save(new ProtoDatabase<OfflinePageEntry>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  for (int64_t id : offline_ids)
    keys_to_remove->push_back(base::Int64ToString(id));

  UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                callback);
}

void OfflinePageMetadataStoreImpl::UpdateEntries(
    std::unique_ptr<ProtoDatabase<OfflinePageEntry>::KeyEntryVector>
        entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    const UpdateCallback& callback) {
  if (!database_.get()) {
    // Failing fast here, because DB is not initialized, and there is nothing
    // that can be done about it.
    // Callback is invoked through message loop to avoid improper retry and
    // simplify testing.
    DVLOG(1) << "Offline pages database not available in UpdateEntries.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }

  database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::Bind(&OfflinePageMetadataStoreImpl::UpdateDone,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageMetadataStoreImpl::UpdateDone(
    const OfflinePageMetadataStore::UpdateCallback& callback,
    bool success) {
  if (!success) {
    // TODO(fgorski): Add UMA for this case. Consider rebuilding the store.
    DVLOG(1) << "Offline pages database update failed.";
  }

  callback.Run(success);
}

void OfflinePageMetadataStoreImpl::Reset(const ResetCallback& callback) {
  database_->Destroy(
      base::Bind(&OfflinePageMetadataStoreImpl::ResetDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void OfflinePageMetadataStoreImpl::ResetDone(
    const ResetCallback& callback,
    bool success) {
  database_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  callback.Run(success);
}

void OfflinePageMetadataStoreImpl::DatabaseUpdateDone(
    const OfflinePageMetadataStore::LoadCallback& cb,
    LoadStatus status,
    const std::vector<OfflinePageItem>& result,
    bool success) {
  // If the update failed, log and keep going.  We'll try to
  // update next time.
  if (!success) {
    LOG(ERROR) << "Failed to update database";
  }
  NotifyLoadResult(cb, status, result);
}

}  // namespace offline_pages
