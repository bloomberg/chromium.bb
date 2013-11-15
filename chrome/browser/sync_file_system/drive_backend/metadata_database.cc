// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

#include <algorithm>
#include <stack>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_entry_kinds.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

struct DatabaseContents {
  scoped_ptr<ServiceMetadata> service_metadata;
  ScopedVector<FileMetadata> file_metadata;
  ScopedVector<FileTracker> file_trackers;
};

namespace {

typedef MetadataDatabase::FileByID FileByID;
typedef MetadataDatabase::TrackerByID TrackerByID;
typedef MetadataDatabase::TrackersByParentAndTitle TrackersByParentAndTitle;
typedef MetadataDatabase::TrackersByTitle TrackersByTitle;

bool IsAppRoot(const FileTracker& tracker) {
  return tracker.tracker_kind() == TRACKER_KIND_APP_ROOT ||
      tracker.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT;
}

std::string RemovePrefix(const std::string& str, const std::string& prefix) {
  if (StartsWithASCII(str, prefix, true))
    return str.substr(prefix.size());
  return str;
}

base::FilePath ReverseConcatPathComponents(
    const std::vector<base::FilePath>& components) {
  if (components.empty())
    return base::FilePath(FILE_PATH_LITERAL("/")).NormalizePathSeparators();

  size_t total_size = 0;
  typedef std::vector<base::FilePath> PathComponents;
  for (PathComponents::const_iterator itr = components.begin();
       itr != components.end(); ++itr)
    total_size += itr->value().size() + 1;

  base::FilePath::StringType result;
  result.reserve(total_size);
  for (PathComponents::const_reverse_iterator itr = components.rbegin();
       itr != components.rend(); ++itr) {
    result.append(1, base::FilePath::kSeparators[0]);
    result.append(itr->value());
  }

  return base::FilePath(result).NormalizePathSeparators();
}

void CreateInitialSyncRootTracker(
    int64 tracker_id,
    const google_apis::FileResource& file_resource,
    scoped_ptr<FileMetadata>* file_out,
    scoped_ptr<FileTracker>* tracker_out) {
  FileDetails details;
  PopulateFileDetailsByFileResource(file_resource, &details);

  scoped_ptr<FileMetadata> file(new FileMetadata);
  file->set_file_id(file_resource.file_id());
  *file->mutable_details() = details;

  scoped_ptr<FileTracker> tracker(new FileTracker);
  tracker->set_tracker_id(tracker_id);
  tracker->set_file_id(file_resource.file_id());
  tracker->set_parent_tracker_id(0);
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  tracker->set_dirty(false);
  tracker->set_active(true);
  tracker->set_needs_folder_listing(false);
  *tracker->mutable_synced_details() = details;

  *file_out = file.Pass();
  *tracker_out = tracker.Pass();
}

void CreateInitialAppRootTracker(
    int64 tracker_id,
    const FileTracker& parent_tracker,
    const google_apis::FileResource& file_resource,
    scoped_ptr<FileMetadata>* file_out,
    scoped_ptr<FileTracker>* tracker_out) {
  FileDetails details;
  PopulateFileDetailsByFileResource(file_resource, &details);

  scoped_ptr<FileMetadata> file(new FileMetadata);
  file->set_file_id(file_resource.file_id());
  *file->mutable_details() = details;

  scoped_ptr<FileTracker> tracker(new FileTracker);
  tracker->set_tracker_id(tracker_id);
  tracker->set_parent_tracker_id(parent_tracker.tracker_id());
  tracker->set_file_id(file_resource.file_id());
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  tracker->set_dirty(false);
  tracker->set_active(false);
  tracker->set_needs_folder_listing(false);
  *tracker->mutable_synced_details() = details;

  *file_out = file.Pass();
  *tracker_out = tracker.Pass();
}

void AdaptLevelDBStatusToSyncStatusCode(const SyncStatusCallback& callback,
                                        const leveldb::Status& status) {
  callback.Run(LevelDBStatusToSyncStatusCode(status));
}

void PutFileDeletionToBatch(const std::string& file_id,
                            leveldb::WriteBatch* batch) {
  batch->Delete(kFileMetadataKeyPrefix + file_id);
}

void PutTrackerDeletionToBatch(int64 tracker_id, leveldb::WriteBatch* batch) {
  batch->Delete(kFileTrackerKeyPrefix + base::Int64ToString(tracker_id));
}

template <typename OutputIterator>
OutputIterator PushChildTrackersToContainer(
    const TrackersByParentAndTitle& trackers_by_parent,
    int64 parent_tracker_id,
    OutputIterator target_itr) {
  TrackersByParentAndTitle::const_iterator found =
      trackers_by_parent.find(parent_tracker_id);
  if (found == trackers_by_parent.end())
    return target_itr;

  for (TrackersByTitle::const_iterator title_itr = found->second.begin();
       title_itr != found->second.end(); ++title_itr) {
    const TrackerSet& trackers = title_itr->second;
    for (TrackerSet::const_iterator tracker_itr = trackers.begin();
         tracker_itr != trackers.end(); ++tracker_itr) {
      *target_itr = (*tracker_itr)->tracker_id();
      ++target_itr;
    }
  }
  return target_itr;
}

std::string GetTrackerTitle(const FileTracker& tracker) {
  if (tracker.has_synced_details())
    return tracker.synced_details().title();
  return std::string();
}

// Returns true if |db| has no content.
bool IsDatabaseEmpty(leveldb::DB* db) {
  DCHECK(db);
  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  itr->SeekToFirst();
  return !itr->Valid();
}

SyncStatusCode OpenDatabase(const base::FilePath& path,
                            scoped_ptr<leveldb::DB>* db_out,
                            bool* created) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db_out);
  DCHECK(created);

  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  leveldb::DB* db = NULL;
  leveldb::Status db_status =
      leveldb::DB::Open(options, path.AsUTF8Unsafe(), &db);
  SyncStatusCode status = LevelDBStatusToSyncStatusCode(db_status);
  if (status != SYNC_STATUS_OK) {
    delete db;
    return status;
  }

  *created = IsDatabaseEmpty(db);
  db_out->reset(db);
  return status;
}

SyncStatusCode MigrateDatabaseIfNeeded(leveldb::DB* db) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  std::string value;
  leveldb::Status status =
      db->Get(leveldb::ReadOptions(), kDatabaseVersionKey, &value);
  int64 version = 0;
  if (status.ok()) {
    if (!base::StringToInt64(value, &version))
      return SYNC_DATABASE_ERROR_FAILED;
  } else {
    if (!status.IsNotFound())
      return SYNC_DATABASE_ERROR_FAILED;
  }

  switch (version) {
    case 0:
      drive_backend::MigrateDatabaseFromV0ToV1(db);
      // fall-through
    case 1:
      drive_backend::MigrateDatabaseFromV1ToV2(db);
      // fall-through
    case 2:
      // TODO(tzik): Migrate database from version 2 to 3.
      //   * Add sync-root folder as active, dirty and needs_folder_listing
      //     folder.
      //   * Add app-root folders for each origins.  Each app-root folder for
      //     an enabled origin should be a active, dirty and
      //     needs_folder_listing folder.  And Each app-root folder for a
      //     disabled origin should be an inactive, dirty and
      //     non-needs_folder_listing folder.
      //   * Add a file metadata for each file in previous version.
      NOTIMPLEMENTED();
      return SYNC_DATABASE_ERROR_FAILED;
      // fall-through
    case 3:
      DCHECK_EQ(3, kCurrentDatabaseVersion);
      return SYNC_STATUS_OK;
    default:
      return SYNC_DATABASE_ERROR_FAILED;
  }
}

SyncStatusCode WriteVersionInfo(leveldb::DB* db) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  return LevelDBStatusToSyncStatusCode(
      db->Put(leveldb::WriteOptions(),
              kDatabaseVersionKey,
              base::Int64ToString(kCurrentDatabaseVersion)));
}

SyncStatusCode ReadDatabaseContents(leveldb::DB* db,
                                    DatabaseContents* contents) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  DCHECK(contents);

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    std::string value = itr->value().ToString();
    if (key == kServiceMetadataKey) {
      scoped_ptr<ServiceMetadata> service_metadata(new ServiceMetadata);
      if (!service_metadata->ParseFromString(value)) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse SyncServiceMetadata");
        continue;
      }

      contents->service_metadata = service_metadata.Pass();
      continue;
    }

    if (StartsWithASCII(key, kFileMetadataKeyPrefix, true)) {
      std::string file_id = RemovePrefix(key, kFileMetadataKeyPrefix);

      scoped_ptr<FileMetadata> file(new FileMetadata);
      if (!file->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a FileMetadata");
        continue;
      }

      contents->file_metadata.push_back(file.release());
      continue;
    }

    if (StartsWithASCII(key, kFileTrackerKeyPrefix, true)) {
      int64 tracker_id = 0;
      if (!base::StringToInt64(RemovePrefix(key, kFileTrackerKeyPrefix),
                               &tracker_id)) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse TrackerID");
        continue;
      }

      scoped_ptr<FileTracker> tracker(new FileTracker);
      if (!tracker->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a Tracker");
        continue;
      }
      contents->file_trackers.push_back(tracker.release());
      continue;
    }
  }

  return SYNC_STATUS_OK;
}

SyncStatusCode InitializeServiceMetadata(DatabaseContents* contents,
                                         leveldb::WriteBatch* batch) {
  if (!contents->service_metadata) {
    contents->service_metadata.reset(new ServiceMetadata);
    contents->service_metadata->set_next_tracker_id(1);

    std::string value;
    contents->service_metadata->SerializeToString(&value);
    batch->Put(kServiceMetadataKey, value);
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode RemoveUnreachableItems(DatabaseContents* contents,
                                      leveldb::WriteBatch* batch) {
  TrackerByID unvisited_trackers;
  typedef std::map<int64, std::set<FileTracker*> > TrackersByParent;
  TrackersByParent trackers_by_parent;

  for (ScopedVector<FileTracker>::iterator itr =
           contents->file_trackers.begin();
       itr != contents->file_trackers.end();
       ++itr) {
    FileTracker* tracker = *itr;
    DCHECK(!ContainsKey(unvisited_trackers, tracker->tracker_id()));
    unvisited_trackers[tracker->tracker_id()] = tracker;
    if (tracker->parent_tracker_id())
      trackers_by_parent[tracker->parent_tracker_id()].insert(tracker);
  }

  // Traverse synced tracker tree. Take only active items, app-root and their
  // children. Drop unreachable items.
  ScopedVector<FileTracker> reachable_trackers;
  std::stack<int64> pending;
  if (contents->service_metadata->sync_root_tracker_id())
    pending.push(contents->service_metadata->sync_root_tracker_id());

  while (!pending.empty()) {
    int64 tracker_id = pending.top();
    pending.pop();

    {
      TrackerByID::iterator found = unvisited_trackers.find(tracker_id);
      if (found == unvisited_trackers.end())
        continue;

      FileTracker* tracker = found->second;
      unvisited_trackers.erase(found);
      reachable_trackers.push_back(tracker);

      if (!tracker->active())
        continue;
    }

    TrackersByParent::iterator found = trackers_by_parent.find(tracker_id);
    if (found == trackers_by_parent.end())
      continue;

    for (std::set<FileTracker*>::const_iterator itr =
             found->second.begin();
         itr != found->second.end();
         ++itr)
      pending.push((*itr)->tracker_id());
  }

  // Delete all unreachable trackers.
  for (TrackerByID::iterator itr = unvisited_trackers.begin();
       itr != unvisited_trackers.end(); ++itr) {
    FileTracker* tracker = itr->second;
    PutTrackerDeletionToBatch(tracker->tracker_id(), batch);
    delete tracker;
  }
  unvisited_trackers.clear();

  // |reachable_trackers| contains all files/folders reachable from sync-root
  // folder via active folders and app-root folders.
  // Update the tracker set in database contents with the reachable tracker set.
  contents->file_trackers.weak_clear();
  contents->file_trackers.swap(reachable_trackers);

  // Do the similar traverse for FileMetadata and remove FileMetadata that don't
  // have reachable trackers.
  FileByID unreferred_files;
  for (ScopedVector<FileMetadata>::const_iterator itr =
           contents->file_metadata.begin();
       itr != contents->file_metadata.end();
       ++itr) {
    unreferred_files.insert(std::make_pair((*itr)->file_id(), *itr));
  }

  ScopedVector<FileMetadata> referred_files;
  for (ScopedVector<FileTracker>::const_iterator itr =
           contents->file_trackers.begin();
       itr != contents->file_trackers.end();
       ++itr) {
    FileByID::iterator found = unreferred_files.find((*itr)->file_id());
    if (found != unreferred_files.end()) {
      referred_files.push_back(found->second);
      unreferred_files.erase(found);
    }
  }

  for (FileByID::iterator itr = unreferred_files.begin();
       itr != unreferred_files.end(); ++itr) {
    FileMetadata* file = itr->second;
    PutFileDeletionToBatch(file->file_id(), batch);
    delete file;
  }
  unreferred_files.clear();

  contents->file_metadata.weak_clear();
  contents->file_metadata.swap(referred_files);

  return SYNC_STATUS_OK;
}

template <typename Container, typename Key, typename Value>
bool FindItem(const Container& container, const Key& key, Value* value) {
  typename Container::const_iterator found = container.find(key);
  if (found == container.end())
    return false;
  if (value)
    *value = *found->second;
  return true;
}

template <typename Container, typename Key>
typename Container::mapped_type FindAndEraseItem(Container* container,
                                                 const Key& key) {
  typedef typename Container::mapped_type Value;
  typename Container::iterator found = container->find(key);
  if (found == container->end())
    return Value();

  Value result = found->second;
  container->erase(found);
  return result;
}

void RunSoon(const tracked_objects::Location& from_here,
             const base::Closure& closure) {
  base::MessageLoopProxy::current()->PostTask(from_here, closure);
}

}  // namespace

bool MetadataDatabase::DirtyTrackerComparator::operator()(
    const FileTracker* left,
    const FileTracker* right) const {
  return left->tracker_id() < right->tracker_id();
}

// static
void MetadataDatabase::Create(base::SequencedTaskRunner* task_runner,
                              const base::FilePath& database_path,
                              const CreateCallback& callback) {
  task_runner->PostTask(FROM_HERE, base::Bind(
      &CreateOnTaskRunner,
      base::MessageLoopProxy::current(),
      make_scoped_refptr(task_runner),
      database_path, callback));
}

MetadataDatabase::~MetadataDatabase() {
  task_runner_->DeleteSoon(FROM_HERE, db_.release());
  STLDeleteContainerPairSecondPointers(
      file_by_id_.begin(), file_by_id_.end());
  STLDeleteContainerPairSecondPointers(
      tracker_by_id_.begin(), tracker_by_id_.end());
}

int64 MetadataDatabase::GetLargestFetchedChangeID() const {
  return service_metadata_->largest_change_id();
}

int64 MetadataDatabase::GetSyncRootTrackerID() const {
  return service_metadata_->sync_root_tracker_id();
}

int64 MetadataDatabase::GetLargestKnownChangeID() const {
  // TODO(tzik): Implement:
  //  - Add |largest_known_file_id| member to hold the value, that should
  //    initially have the same value to |largest_change_id|.
  //  - Change UpdateByFileResource and UpdateByChangeList not to overwrite
  //    FileMetadata if the newer one.
  //  - Change ListChangesTask to set UpdateLargestKnownChangeID.
  return GetLargestFetchedChangeID();
}

void MetadataDatabase::UpdateLargestKnownChangeID(int64 change_id) {
  NOTIMPLEMENTED();
}

bool MetadataDatabase::HasSyncRoot() const {
  return service_metadata_->has_sync_root_tracker_id() &&
      !!service_metadata_->sync_root_tracker_id();
}

void MetadataDatabase::PopulateInitialData(
    int64 largest_change_id,
    const google_apis::FileResource& sync_root_folder,
    const ScopedVector<google_apis::FileResource>& app_root_folders,
    const SyncStatusCallback& callback) {
  DCHECK(tracker_by_id_.empty());
  DCHECK(file_by_id_.empty());

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  service_metadata_->set_largest_change_id(largest_change_id);

  FileTracker* sync_root_tracker = NULL;
  int64 sync_root_tracker_id = 0;
  {
    scoped_ptr<FileMetadata> folder;
    scoped_ptr<FileTracker> tracker;
    CreateInitialSyncRootTracker(GetNextTrackerID(batch.get()),
                                 sync_root_folder,
                                 &folder,
                                 &tracker);
    std::string sync_root_folder_id = folder->file_id();
    sync_root_tracker = tracker.get();
    sync_root_tracker_id = tracker->tracker_id();

    PutFileToBatch(*folder, batch.get());
    PutTrackerToBatch(*tracker, batch.get());

    service_metadata_->set_sync_root_tracker_id(tracker->tracker_id());
    PutServiceMetadataToBatch(*service_metadata_, batch.get());

    trackers_by_file_id_[folder->file_id()].Insert(tracker.get());

    file_by_id_[sync_root_folder_id] = folder.release();
    tracker_by_id_[sync_root_tracker_id] = tracker.release();
  }

  for (ScopedVector<google_apis::FileResource>::const_iterator itr =
           app_root_folders.begin();
       itr != app_root_folders.end();
       ++itr) {
    const google_apis::FileResource& folder_resource = **itr;
    scoped_ptr<FileMetadata> folder;
    scoped_ptr<FileTracker> tracker;
    CreateInitialAppRootTracker(GetNextTrackerID(batch.get()),
                                *sync_root_tracker,
                                folder_resource,
                                &folder,
                                &tracker);
    std::string title = folder->details().title();
    std::string folder_id = folder->file_id();
    int64 tracker_id = tracker->tracker_id();

    PutFileToBatch(*folder, batch.get());
    PutTrackerToBatch(*tracker, batch.get());

    trackers_by_file_id_[folder_id].Insert(tracker.get());
    trackers_by_parent_and_title_[sync_root_tracker_id][title]
        .Insert(tracker.get());

    file_by_id_[folder_id] = folder.release();
    tracker_by_id_[tracker_id] = tracker.release();
  }

  WriteToDatabase(batch.Pass(), callback);
}


void MetadataDatabase::RegisterApp(const std::string& app_id,
                                   const std::string& folder_id,
                                   const SyncStatusCallback& callback) {
  if (FindAppRootTracker(app_id, NULL)) {
    // The app-root is already registered.
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  TrackerSet trackers;
  if (!FindTrackersByFileID(folder_id, &trackers) || trackers.has_active()) {
    // The folder is tracked by another tracker.
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to register App for %s", app_id.c_str());
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_HAS_CONFLICT));
    return;
  }

  int64 sync_root_tracker_id = service_metadata_->sync_root_tracker_id();
  if (!sync_root_tracker_id) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Sync-root needs to be set up before registering app-root");
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  // Make this tracker an app-root tracker.
  FileTracker* app_root_tracker = NULL;
  for (TrackerSet::iterator itr = trackers.begin();
       itr != trackers.end(); ++itr) {
    FileTracker* tracker = *itr;
    if (tracker->parent_tracker_id() == sync_root_tracker_id)
      app_root_tracker = tracker;
  }

  if (!app_root_tracker) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  RegisterTrackerAsAppRoot(app_id, app_root_tracker->tracker_id(), batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::DisableApp(const std::string& app_id,
                                  const SyncStatusCallback& callback) {
  FileTracker tracker;
  if (!FindAppRootTracker(app_id, &tracker)) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  if (tracker.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  MakeAppRootDisabled(tracker.tracker_id(), batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::EnableApp(const std::string& app_id,
                                 const SyncStatusCallback& callback) {
  FileTracker tracker;
  if (!FindAppRootTracker(app_id, &tracker) ||
      tracker.tracker_kind() == TRACKER_KIND_REGULAR) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  if (tracker.tracker_kind() == TRACKER_KIND_APP_ROOT) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  MakeAppRootEnabled(tracker.tracker_id(), batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::UnregisterApp(const std::string& app_id,
                                     const SyncStatusCallback& callback) {
  FileTracker tracker;
  if (!FindAppRootTracker(app_id, &tracker) ||
      tracker.tracker_kind() == TRACKER_KIND_REGULAR) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  UnregisterTrackerAsAppRoot(app_id, batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

bool MetadataDatabase::FindAppRootTracker(const std::string& app_id,
                                          FileTracker* tracker) const {
  return FindItem(app_root_by_app_id_, app_id, tracker);
}

bool MetadataDatabase::FindFileByFileID(const std::string& file_id,
                                        FileMetadata* file) const {
  return FindItem(file_by_id_, file_id, file);
}

bool MetadataDatabase::FindTrackersByFileID(const std::string& file_id,
                                            TrackerSet* trackers) const {
  TrackersByFileID::const_iterator found = trackers_by_file_id_.find(file_id);
  if (found == trackers_by_file_id_.end())
    return false;
  if (trackers)
    *trackers = found->second;
  return true;
}

bool MetadataDatabase::FindTrackersByParentAndTitle(
    int64 parent_tracker_id,
    const std::string& title,
    TrackerSet* trackers) const {
  TrackersByParentAndTitle::const_iterator found_by_parent =
      trackers_by_parent_and_title_.find(parent_tracker_id);
  if (found_by_parent == trackers_by_parent_and_title_.end())
    return false;

  TrackersByTitle::const_iterator found_by_title =
      found_by_parent->second.find(title);
  if (found_by_title == found_by_parent->second.end())
    return false;

  if (trackers)
    *trackers = found_by_title->second;
  return true;
}

bool MetadataDatabase::FindTrackerByTrackerID(int64 tracker_id,
                                              FileTracker* tracker) const {
  return FindItem(tracker_by_id_, tracker_id, tracker);
}

bool MetadataDatabase::BuildPathForTracker(int64 tracker_id,
                                           base::FilePath* path) const {
  FileTracker current;
  if (!FindTrackerByTrackerID(tracker_id, &current) || !current.active())
    return false;

  std::vector<base::FilePath> components;
  while (!IsAppRoot(current)) {
    std::string title = GetTrackerTitle(current);
    if (title.empty())
      return false;
    components.push_back(base::FilePath::FromUTF8Unsafe(title));
    if (!FindTrackerByTrackerID(current.parent_tracker_id(), &current) ||
        !current.active())
      return false;
  }

  if (path)
    *path = ReverseConcatPathComponents(components);

  return true;
}

void MetadataDatabase::UpdateByChangeList(
    int64 largest_change_id,
    ScopedVector<google_apis::ChangeResource> changes,
    const SyncStatusCallback& callback) {
  DCHECK_LE(service_metadata_->largest_change_id(), largest_change_id);

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);

  for (ScopedVector<google_apis::ChangeResource>::const_iterator itr =
           changes.begin();
       itr != changes.end();
       ++itr) {
    const google_apis::ChangeResource& change = **itr;
    scoped_ptr<FileMetadata> file(
        CreateFileMetadataFromChangeResource(change));
    std::string file_id = file->file_id();

    MarkTrackersDirtyByFileID(file_id, batch.get());
    if (!file->details().deleted())
      MaybeAddTrackersForNewFile(*file, batch.get());

    if (FindTrackersByFileID(file_id, NULL)) {
      PutFileToBatch(*file, batch.get());

      // Set |file| to |file_by_id_[file_id]| and delete old value.
      FileMetadata* file_ptr = file.release();
      std::swap(file_ptr, file_by_id_[file_id]);
      delete file_ptr;
    }
  }

  service_metadata_->set_largest_change_id(largest_change_id);
  PutServiceMetadataToBatch(*service_metadata_, batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::UpdateByFileResource(
    int64 change_id,
    const google_apis::FileResource& resource,
    const SyncStatusCallback& callback) {
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);

  scoped_ptr<FileMetadata> file(
      CreateFileMetadataFromFileResource(change_id, resource));
  std::string file_id = file->file_id();

  // TODO(tzik): Consolidate with UpdateByChangeList.
  MarkTrackersDirtyByFileID(file_id, batch.get());
  if (!file->details().deleted()) {
    MaybeAddTrackersForNewFile(*file, batch.get());

    if (FindTrackersByFileID(file_id, NULL)) {
      PutFileToBatch(*file, batch.get());

      // Set |file| to |file_by_id_[file_id]| and delete old value.
      FileMetadata* file_ptr = file.release();
      std::swap(file_ptr, file_by_id_[file_id]);
      delete file_ptr;
    }
  }

  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::PopulateFolderByChildList(
    const std::string& folder_id,
    const FileIDList& child_file_ids,
    const SyncStatusCallback& callback) {
  TrackerSet trackers;
  if (!FindTrackersByFileID(folder_id, &trackers) ||
      !trackers.has_active()) {
    // It's OK that there is no folder to populate its children.
    // Inactive folders should ignore their contents updates.
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  FileTracker* folder_tracker =
      tracker_by_id_[trackers.active_tracker()->tracker_id()];
  DCHECK(folder_tracker);
  std::set<std::string> children(child_file_ids.begin(), child_file_ids.end());

  std::vector<int64> known_children;
  PushChildTrackersToContainer(trackers_by_parent_and_title_,
                               folder_tracker->tracker_id(),
                               std::back_inserter(known_children));
  for (std::vector<int64>::iterator itr = known_children.begin();
       itr != known_children.end(); ++itr)
    children.erase(tracker_by_id_[*itr]->file_id());

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  for (FileIDList::const_iterator itr = child_file_ids.begin();
       itr != child_file_ids.end(); ++itr)
    CreateTrackerForParentAndFileID(*folder_tracker, *itr, batch.get());
  folder_tracker->set_needs_folder_listing(false);
  if (folder_tracker->dirty() && !ShouldKeepDirty(*folder_tracker)) {
    folder_tracker->set_dirty(false);
    dirty_trackers_.erase(folder_tracker);
  }
  PutTrackerToBatch(*folder_tracker, batch.get());

  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::UpdateTracker(int64 tracker_id,
                                     const FileDetails& updated_details,
                                     const SyncStatusCallback& callback) {
  TrackerByID::iterator found = tracker_by_id_.find(tracker_id);
  if (found == tracker_by_id_.end()) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  FileTracker* tracker = found->second;
  DCHECK(tracker);

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);

  if (updated_details.deleted()) {
    // The update deletes the local file.
    FileByID::iterator found = file_by_id_.find(tracker->file_id());
    if (found == file_by_id_.end() || found->second->details().deleted()) {
      // Both the tracker and metadata have the deleted flag, now it's safe to
      // delete the |tracker|.
      RemoveTracker(tracker->tracker_id(), batch.get());
    } else {
      // The local file is deleted, but corresponding remote file isn't.
      // Put the tracker back to the initial state.
      tracker->clear_synced_details();
      tracker->set_dirty(true);
      tracker->set_active(false);
      PutTrackerToBatch(*tracker, batch.get());
    }

    WriteToDatabase(batch.Pass(), callback);
    return;
  }

  // Check if the tracker was retitled.  If it was, update the title and its
  // index in advance.
  if (!tracker->has_synced_details() ||
      tracker->synced_details().title() != updated_details.title()) {
    UpdateTrackerTitle(tracker, updated_details.title(), batch.get());
  }

  *tracker->mutable_synced_details() = updated_details;

  // Activate the tracker if:
  //   - There is no active tracker that tracks |tracker->file_id()|.
  //   - There is no active tracker that has the same |parent| and |title|.
  if (!tracker->active() && CanActivateTracker(*tracker))
    MakeTrackerActive(tracker->tracker_id(), batch.get());
  if (tracker->dirty() && !ShouldKeepDirty(*tracker)) {
    tracker->set_dirty(false);
    dirty_trackers_.erase(tracker);
  }
  PutTrackerToBatch(*tracker, batch.get());

  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::LowerTrackerPriority(int64 tracker_id) {
  // TODO(tzik): Move the tracker from |normal_priority_dirty_trackers_| to
  // |low_priority_dirty_trackers|.
  NOTIMPLEMENTED();
}

bool MetadataDatabase::GetNormalPriorityDirtyTracker(FileTracker* tracker) {
  // TODO(tzik): Split |dirty_trackers| to |normal_priority_dirty_trackers|
  // and |low_priority_dirty_trackers|.
  // Add a function to mark a dirty tracker low priority.
  NOTIMPLEMENTED();
  return false;
}

bool MetadataDatabase::GetLowPriorityDirtyTracker(FileTracker* tracker) {
  NOTIMPLEMENTED();
  return false;
}

MetadataDatabase::MetadataDatabase(base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {
  DCHECK(task_runner);
}

// static
void MetadataDatabase::CreateOnTaskRunner(
    base::SingleThreadTaskRunner* callback_runner,
    base::SequencedTaskRunner* task_runner,
    const base::FilePath& database_path,
    const CreateCallback& callback) {
  scoped_ptr<MetadataDatabase> metadata_database(
      new MetadataDatabase(task_runner));
  SyncStatusCode status =
      metadata_database->InitializeOnTaskRunner(database_path);
  if (status != SYNC_STATUS_OK)
    metadata_database.reset();

  callback_runner->PostTask(FROM_HERE, base::Bind(
      callback, status, base::Passed(&metadata_database)));
}

// static
SyncStatusCode MetadataDatabase::CreateForTesting(
    scoped_ptr<leveldb::DB> db,
    scoped_ptr<MetadataDatabase>* metadata_database_out) {
  scoped_ptr<MetadataDatabase> metadata_database(
      new MetadataDatabase(base::MessageLoopProxy::current()));
  metadata_database->db_ = db.Pass();
  SyncStatusCode status =
      metadata_database->InitializeOnTaskRunner(base::FilePath());
  if (status == SYNC_STATUS_OK)
    *metadata_database_out = metadata_database.Pass();
  return status;
}

SyncStatusCode MetadataDatabase::SetLargestChangeIDForTesting(
    int64 largest_change_id) {
  service_metadata_->set_largest_change_id(largest_change_id);

  leveldb::WriteBatch batch;
  PutServiceMetadataToBatch(*service_metadata_, &batch);
  return LevelDBStatusToSyncStatusCode(
      db_->Write(leveldb::WriteOptions(), &batch));
}

SyncStatusCode MetadataDatabase::InitializeOnTaskRunner(
    const base::FilePath& database_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  SyncStatusCode status = SYNC_STATUS_UNKNOWN;
  bool created = false;
  // Open database unless |db_| is overridden for testing.
  if (!db_) {
    status = OpenDatabase(database_path, &db_, &created);
    if (status != SYNC_STATUS_OK)
      return status;
  }

  if (created) {
    status = WriteVersionInfo(db_.get());
    if (status != SYNC_STATUS_OK)
      return status;
  } else {
    status = MigrateDatabaseIfNeeded(db_.get());
    if (status != SYNC_STATUS_OK)
      return status;
  }

  DatabaseContents contents;
  status = ReadDatabaseContents(db_.get(), &contents);
  if (status != SYNC_STATUS_OK)
    return status;

  leveldb::WriteBatch batch;
  status = InitializeServiceMetadata(&contents, &batch);
  if (status != SYNC_STATUS_OK)
    return status;

  status = RemoveUnreachableItems(&contents, &batch);
  if (status != SYNC_STATUS_OK)
    return status;

  status = LevelDBStatusToSyncStatusCode(
      db_->Write(leveldb::WriteOptions(), &batch));
  if (status != SYNC_STATUS_OK)
    return status;

  BuildIndexes(&contents);
  return status;
}

void MetadataDatabase::BuildIndexes(DatabaseContents* contents) {
  service_metadata_ = contents->service_metadata.Pass();

  for (ScopedVector<FileMetadata>::const_iterator itr =
           contents->file_metadata.begin();
       itr != contents->file_metadata.end();
       ++itr) {
    file_by_id_[(*itr)->file_id()] = *itr;
  }
  contents->file_metadata.weak_clear();

  for (ScopedVector<FileTracker>::const_iterator itr =
           contents->file_trackers.begin();
       itr != contents->file_trackers.end();
       ++itr) {
    FileTracker* tracker = *itr;
    tracker_by_id_[tracker->tracker_id()] = tracker;
    trackers_by_file_id_[tracker->file_id()].Insert(tracker);

    if (IsAppRoot(*tracker))
      app_root_by_app_id_[tracker->app_id()] = tracker;

    if (tracker->parent_tracker_id()) {
      std::string title = GetTrackerTitle(*tracker);
      TrackerSet* trackers =
          &trackers_by_parent_and_title_[tracker->parent_tracker_id()][title];
      trackers->Insert(tracker);
    }

    if (tracker->dirty())
      dirty_trackers_.insert(tracker);
  }
  contents->file_trackers.weak_clear();
}

void MetadataDatabase::RegisterTrackerAsAppRoot(
    const std::string& app_id,
    int64 tracker_id,
    leveldb::WriteBatch* batch) {
  FileTracker* tracker = tracker_by_id_[tracker_id];
  DCHECK(tracker);
  tracker->set_app_id(app_id);
  tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);
  app_root_by_app_id_[app_id] = tracker;

  MakeTrackerActive(tracker->tracker_id(), batch);
}

void MetadataDatabase::UnregisterTrackerAsAppRoot(
    const std::string& app_id,
    leveldb::WriteBatch* batch) {
  FileTracker* tracker = FindAndEraseItem(&app_root_by_app_id_, app_id);
  tracker->set_app_id(std::string());
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);

  // Inactivate the tracker to drop all descendant.
  // (Note that we set tracker_kind to TRACKER_KIND_REGULAR before calling
  // this.)
  MakeTrackerInactive(tracker->tracker_id(), batch);
}

void MetadataDatabase::MakeTrackerActive(int64 tracker_id,
                                         leveldb::WriteBatch* batch) {
  FileTracker* tracker = tracker_by_id_[tracker_id];
  DCHECK(tracker);
  DCHECK(!tracker->active());

  int64 parent_tracker_id = tracker->parent_tracker_id();
  DCHECK(tracker->has_synced_details());
  trackers_by_file_id_[tracker->file_id()].Activate(tracker);
  if (parent_tracker_id) {
    trackers_by_parent_and_title_[parent_tracker_id][
        tracker->synced_details().title()].Activate(tracker);
  }
  tracker->set_active(true);
  tracker->set_needs_folder_listing(
      tracker->synced_details().file_kind() == FILE_KIND_FOLDER);
  tracker->set_dirty(true);
  dirty_trackers_.insert(tracker);

  PutTrackerToBatch(*tracker, batch);
}

void MetadataDatabase::MakeTrackerInactive(int64 tracker_id,
                                           leveldb::WriteBatch* batch) {
  FileTracker* tracker = tracker_by_id_[tracker_id];
  DCHECK(tracker);
  DCHECK(tracker->active());
  DCHECK_EQ(TRACKER_KIND_REGULAR, tracker->tracker_kind());
  trackers_by_file_id_[tracker->file_id()].Inactivate(tracker);

  std::string title = GetTrackerTitle(*tracker);
  int64 parent_tracker_id = tracker->parent_tracker_id();
  if (parent_tracker_id)
    trackers_by_parent_and_title_[parent_tracker_id][title].Inactivate(tracker);
  tracker->set_active(false);

  RemoveAllDescendantTrackers(tracker_id, batch);
  MarkTrackersDirtyByFileID(tracker->file_id(), batch);
  if (parent_tracker_id)
    MarkTrackersDirtyByPath(parent_tracker_id, title, batch);
  PutTrackerToBatch(*tracker, batch);
}

void MetadataDatabase::MakeAppRootDisabled(int64 tracker_id,
                                           leveldb::WriteBatch* batch) {
  FileTracker* tracker = tracker_by_id_[tracker_id];
  DCHECK(tracker);
  DCHECK_EQ(TRACKER_KIND_APP_ROOT, tracker->tracker_kind());
  DCHECK(tracker->active());

  // Keep the app-root tracker active (but change the tracker_kind) so that
  // other conflicting trackers won't become active.
  tracker->set_tracker_kind(TRACKER_KIND_DISABLED_APP_ROOT);
  PutTrackerToBatch(*tracker, batch);
}

void MetadataDatabase::MakeAppRootEnabled(int64 tracker_id,
                                          leveldb::WriteBatch* batch) {
  FileTracker* tracker = tracker_by_id_[tracker_id];
  DCHECK(tracker);
  DCHECK_EQ(TRACKER_KIND_DISABLED_APP_ROOT, tracker->tracker_kind());
  DCHECK(tracker->active());

  tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);
  // Mark descendant trackers as dirty to handle changes in disable period.
  RecursiveMarkTrackerAsDirty(tracker_id, batch);
  PutTrackerToBatch(*tracker, batch);
}

void MetadataDatabase::CreateTrackerForParentAndFileID(
    const FileTracker& parent_tracker,
    const std::string& file_id,
    leveldb::WriteBatch* batch) {
  int64 tracker_id = GetNextTrackerID(batch);
  scoped_ptr<FileTracker> tracker(new FileTracker);
  tracker->set_tracker_id(tracker_id);
  tracker->set_parent_tracker_id(parent_tracker.tracker_id());
  tracker->set_file_id(file_id);
  tracker->set_app_id(parent_tracker.app_id());
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  tracker->set_dirty(true);
  tracker->set_active(false);
  tracker->set_needs_folder_listing(false);
  PutTrackerToBatch(*tracker, batch);

  trackers_by_file_id_[file_id].Insert(tracker.get());
  // Note: |trackers_by_parent_and_title_| does not map from
  // FileMetadata::details but from FileTracker::synced_details, which is filled
  // on tracker updated phase.  Use empty string as the title since
  // FileTracker::synced_details is empty here.
  trackers_by_parent_and_title_[parent_tracker.tracker_id()][std::string()]
      .Insert(tracker.get());
  dirty_trackers_.insert(tracker.get());
  DCHECK(!ContainsKey(tracker_by_id_, tracker_id));
  tracker_by_id_[tracker_id] = tracker.release();
}

void MetadataDatabase::RemoveTracker(int64 tracker_id,
                                     leveldb::WriteBatch* batch) {
  RemoveTrackerInternal(tracker_id, batch, false);
}

void MetadataDatabase::RemoveTrackerIgnoringSiblings(
    int64 tracker_id,
    leveldb::WriteBatch* batch) {
  RemoveTrackerInternal(tracker_id, batch, true);
}

void MetadataDatabase::RemoveTrackerInternal(
    int64 tracker_id,
    leveldb::WriteBatch* batch,
    bool ignoring_siblings) {
  scoped_ptr<FileTracker> tracker(
      FindAndEraseItem(&tracker_by_id_, tracker_id));
  if (!tracker)
    return;

  EraseTrackerFromFileIDIndex(tracker.get(), batch);
  if (IsAppRoot(*tracker))
    app_root_by_app_id_.erase(tracker->app_id());
  EraseTrackerFromPathIndex(tracker.get());

  MarkTrackersDirtyByFileID(tracker->file_id(), batch);
  if (!ignoring_siblings) {
    MarkTrackersDirtyByPath(tracker->parent_tracker_id(),
                            GetTrackerTitle(*tracker),
                            batch);
  }
  PutTrackerDeletionToBatch(tracker_id, batch);
}

void MetadataDatabase::MaybeAddTrackersForNewFile(
    const FileMetadata& file,
    leveldb::WriteBatch* batch) {
  std::set<int64> known_parents;
  TrackersByFileID::iterator found = trackers_by_file_id_.find(file.file_id());
  if (found != trackers_by_file_id_.end()) {
    for (TrackerSet::const_iterator itr = found->second.begin();
         itr != found->second.end(); ++itr) {
      int64 parent_tracker_id = (*itr)->parent_tracker_id();
      if (parent_tracker_id)
        known_parents.insert(parent_tracker_id);
    }
  }

  for (int i = 0; i < file.details().parent_folder_ids_size(); ++i) {
    std::string parent_folder_id = file.details().parent_folder_ids(i);
    TrackersByFileID::iterator found =
        trackers_by_file_id_.find(parent_folder_id);
    if (found == trackers_by_file_id_.end())
      continue;

    for (TrackerSet::const_iterator itr = found->second.begin();
         itr != found->second.end(); ++itr) {
      FileTracker* parent_tracker = *itr;
      int64 parent_tracker_id = parent_tracker->tracker_id();
      if (!parent_tracker->active())
        continue;

      if (ContainsKey(known_parents, parent_tracker_id))
        continue;

      CreateTrackerForParentAndFileID(*parent_tracker, file.file_id(), batch);
    }
  }
}

void MetadataDatabase::RemoveAllDescendantTrackers(int64 root_tracker_id,
                                                   leveldb::WriteBatch* batch) {
  std::vector<int64> pending_trackers;
  PushChildTrackersToContainer(trackers_by_parent_and_title_,
                               root_tracker_id,
                               std::back_inserter(pending_trackers));

  while (!pending_trackers.empty()) {
    int64 tracker_id = pending_trackers.back();
    pending_trackers.pop_back();
    PushChildTrackersToContainer(trackers_by_parent_and_title_,
                                 tracker_id,
                                 std::back_inserter(pending_trackers));
    RemoveTrackerIgnoringSiblings(tracker_id, batch);
  }
}

void MetadataDatabase::EraseTrackerFromFileIDIndex(FileTracker* tracker,
                                                   leveldb::WriteBatch* batch) {
  TrackersByFileID::iterator found =
      trackers_by_file_id_.find(tracker->file_id());
  if (found == trackers_by_file_id_.end())
    return;

  TrackerSet* trackers = &found->second;
  trackers->Erase(tracker);
  if (!trackers->tracker_set().empty())
    return;
  trackers_by_file_id_.erase(found);
  EraseFileFromDatabase(tracker->file_id(), batch);
}

void MetadataDatabase::EraseFileFromDatabase(const std::string& file_id,
                                             leveldb::WriteBatch* batch) {
  scoped_ptr<FileMetadata> file(FindAndEraseItem(&file_by_id_, file_id));
  if (file)
    PutFileDeletionToBatch(file_id, batch);
}

void MetadataDatabase::EraseTrackerFromPathIndex(FileTracker* tracker) {
  TrackersByParentAndTitle::iterator found =
      trackers_by_parent_and_title_.find(tracker->parent_tracker_id());
  if (found == trackers_by_parent_and_title_.end())
    return;

  std::string title = GetTrackerTitle(*tracker);
  TrackersByTitle* trackers_by_title = &found->second;
  TrackersByTitle::iterator found_by_title = trackers_by_title->find(title);
  TrackerSet* conflicting_trackers = &found_by_title->second;
  conflicting_trackers->Erase(tracker);

  if (conflicting_trackers->tracker_set().empty()) {
    trackers_by_title->erase(found_by_title);
    if (trackers_by_title->empty())
      trackers_by_parent_and_title_.erase(found);
  }
}

void MetadataDatabase::MarkTrackerSetDirty(
    TrackerSet* trackers,
    leveldb::WriteBatch* batch) {
  for (TrackerSet::iterator itr = trackers->begin();
       itr != trackers->end(); ++itr) {
    FileTracker* tracker = *itr;
    if (tracker->dirty())
      continue;
    tracker->set_dirty(true);
    PutTrackerToBatch(*tracker, batch);
    dirty_trackers_.insert(tracker);
  }
}

void MetadataDatabase::MarkTrackersDirtyByFileID(
    const std::string& file_id,
    leveldb::WriteBatch* batch) {
  TrackersByFileID::iterator found = trackers_by_file_id_.find(file_id);
  if (found != trackers_by_file_id_.end())
    MarkTrackerSetDirty(&found->second, batch);
}

void MetadataDatabase::MarkTrackersDirtyByPath(int64 parent_tracker_id,
                                               const std::string& title,
                                               leveldb::WriteBatch* batch) {
  TrackersByParentAndTitle::iterator found =
      trackers_by_parent_and_title_.find(parent_tracker_id);
  if (found == trackers_by_parent_and_title_.end()) {
    NOTREACHED() << "parent: " << parent_tracker_id
                 << ", title: " << title;
    return;
  }

  TrackersByTitle::iterator itr = found->second.find(title);
  if (itr != found->second.end())
    MarkTrackerSetDirty(&itr->second, batch);
}

int64 MetadataDatabase::GetNextTrackerID(leveldb::WriteBatch* batch) {
  int64 tracker_id = service_metadata_->next_tracker_id();
  service_metadata_->set_next_tracker_id(tracker_id + 1);
  PutServiceMetadataToBatch(*service_metadata_, batch);
  DCHECK_GT(tracker_id, 0);
  return tracker_id;
}

void MetadataDatabase::RecursiveMarkTrackerAsDirty(int64 root_tracker_id,
                                                   leveldb::WriteBatch* batch) {
  std::vector<int64> stack;
  stack.push_back(root_tracker_id);
  while (!stack.empty()) {
    int64 tracker_id = stack.back();
    stack.pop_back();
    PushChildTrackersToContainer(
        trackers_by_parent_and_title_, tracker_id, std::back_inserter(stack));

    FileTracker* tracker = tracker_by_id_[tracker_id];
    if (!tracker->dirty()) {
      tracker->set_dirty(true);
      PutTrackerToBatch(*tracker, batch);
      dirty_trackers_.insert(tracker);
    }
  }
}

bool MetadataDatabase::CanActivateTracker(const FileTracker& tracker) {
  DCHECK(!tracker.active());
  DCHECK_NE(service_metadata_->sync_root_tracker_id(), tracker.tracker_id());

  if (HasActiveTrackerForFileID(tracker.file_id()))
    return false;

  if (tracker.app_id().empty())
    return false;
  if (!tracker.has_synced_details())
    return false;
  DCHECK(tracker.parent_tracker_id());

  return !HasActiveTrackerForPath(tracker.parent_tracker_id(),
                                  tracker.synced_details().title());
}

bool MetadataDatabase::ShouldKeepDirty(const FileTracker& tracker) const {
  if (HasDisabledAppRoot(tracker))
    return false;

  DCHECK(tracker.dirty());
  if (!tracker.has_synced_details())
    return true;

  FileByID::const_iterator found = file_by_id_.find(tracker.file_id());
  if (found == file_by_id_.end())
    return true;
  const FileMetadata* file = found->second;
  DCHECK(file);

  if (tracker.active()) {
    if (tracker.needs_folder_listing())
      return true;
    if (tracker.synced_details().md5() != file->details().md5())
      return true;
  }

  const FileDetails& local_details = tracker.synced_details();
  const FileDetails& remote_details = file->details();

  if (local_details.title() != remote_details.title())
    return true;
  if (local_details.deleted() != remote_details.deleted())
    return true;

  return false;
}

bool MetadataDatabase::HasDisabledAppRoot(const FileTracker& tracker) const {
  TrackerByAppID::const_iterator found =
      app_root_by_app_id_.find(tracker.app_id());
  if (found == app_root_by_app_id_.end())
    return false;

  const FileTracker* app_root_tracker = found->second;
  DCHECK(app_root_tracker);
  return app_root_tracker->tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT;
}

bool MetadataDatabase::HasActiveTrackerForFileID(
    const std::string& file_id) const {
  TrackersByFileID::const_iterator found = trackers_by_file_id_.find(file_id);
  return found != trackers_by_file_id_.end() && found->second.has_active();
}

bool MetadataDatabase::HasActiveTrackerForPath(int64 parent_tracker_id,
                                               const std::string& title) const {
  TrackersByParentAndTitle::const_iterator found_by_parent =
      trackers_by_parent_and_title_.find(parent_tracker_id);
  if (found_by_parent == trackers_by_parent_and_title_.end())
    return false;

  const TrackersByTitle& trackers_by_title = found_by_parent->second;
  TrackersByTitle::const_iterator found = trackers_by_title.find(title);
  return found != trackers_by_title.end() && found->second.has_active();
}

void MetadataDatabase::UpdateTrackerTitle(FileTracker* tracker,
                                          const std::string& new_title,
                                          leveldb::WriteBatch* batch) {
  int64 parent_id = tracker->parent_tracker_id();
  std::string old_title = GetTrackerTitle(*tracker);
  DCHECK_NE(old_title, new_title);
  DCHECK(!new_title.empty());

  TrackersByTitle* trackers_by_title =
      &trackers_by_parent_and_title_[parent_id];
  TrackerSet* old_siblings = &(*trackers_by_title)[old_title];
  TrackerSet* new_siblings = &(*trackers_by_title)[new_title];

  old_siblings->Erase(tracker);
  if (old_siblings->empty())
    trackers_by_title->erase(old_title);
  else
    MarkTrackerSetDirty(old_siblings, batch);

  if (tracker->active() && new_siblings->has_active()) {
    // Inactivate existing active tracker.
    FileTracker* obstacle = new_siblings->active_tracker();
    new_siblings->Inactivate(obstacle);
    DCHECK_EQ(TRACKER_KIND_REGULAR, obstacle->tracker_kind());

    TrackerSet* same_file_id_trackers_to_obstacle =
        &trackers_by_file_id_[obstacle->file_id()];
    same_file_id_trackers_to_obstacle->Inactivate(obstacle);
    MarkTrackerSetDirty(same_file_id_trackers_to_obstacle, batch);

    obstacle->set_active(false);
    PutTrackerToBatch(*obstacle, batch);

    RemoveAllDescendantTrackers(obstacle->tracker_id(), batch);
  }

  tracker->mutable_synced_details()->set_title(new_title);
  new_siblings->Insert(tracker);
  PutTrackerToBatch(*tracker, batch);
}

void MetadataDatabase::WriteToDatabase(scoped_ptr<leveldb::WriteBatch> batch,
                                       const SyncStatusCallback& callback) {
  base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&leveldb::DB::Write,
                 base::Unretained(db_.get()),
                 leveldb::WriteOptions(),
                 base::Owned(batch.release())),
      base::Bind(&AdaptLevelDBStatusToSyncStatusCode, callback));
}

scoped_ptr<base::ListValue> MetadataDatabase::DumpFiles(
    const std::string& app_id) {
  scoped_ptr<base::ListValue> files(new base::ListValue);

  FileTracker app_root_tracker;
  if (!FindAppRootTracker(app_id, &app_root_tracker))
    return files.Pass();

  std::vector<int64> stack;
  PushChildTrackersToContainer(
        trackers_by_parent_and_title_,
        app_root_tracker.tracker_id(),
        std::back_inserter(stack));
  while (!stack.empty()) {
    int64 tracker_id = stack.back();
    stack.pop_back();
    PushChildTrackersToContainer(
        trackers_by_parent_and_title_, tracker_id, std::back_inserter(stack));

    FileTracker* tracker = tracker_by_id_[tracker_id];
    base::DictionaryValue* file = new DictionaryValue;

    base::FilePath path;
    if (tracker->active()) {
      BuildPathForTracker(tracker->tracker_id(), &path);
    } else {
      BuildPathForTracker(tracker->parent_tracker_id(), &path);
      if (tracker->has_synced_details()) {
        path = path.Append(
            base::FilePath::FromUTF8Unsafe(tracker->synced_details().title()));
      } else {
        path = path.Append(FILE_PATH_LITERAL("unknown"));
      }
    }
    file->SetString("path", path.AsUTF8Unsafe());
    if (tracker->has_synced_details()) {
      file->SetString("title", tracker->synced_details().title());
      file->SetString("type",
                      FileKindToString(tracker->synced_details().file_kind()));
    }

    base::DictionaryValue* details = new DictionaryValue;
    details->SetString("file_id", tracker->file_id());
    if (tracker->has_synced_details() &&
        tracker->synced_details().file_kind() == FILE_KIND_FILE)
      details->SetString("md5",tracker->synced_details().md5());
    details->SetString("active", tracker->active() ? "true" : "false");
    details->SetString("dirty", tracker->dirty() ? "true" : "false");

    file->Set("details", details);

    files->Append(file);
  }

  return files.Pass();
}

}  // namespace drive_backend
}  // namespace sync_file_system
