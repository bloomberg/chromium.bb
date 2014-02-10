// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

#include <algorithm>
#include <stack>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
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
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_entry_kinds.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
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

scoped_ptr<FileTracker> CreateSyncRootTracker(
    int64 tracker_id,
    const FileMetadata& sync_root_metadata) {
  scoped_ptr<FileTracker> sync_root_tracker(new FileTracker);
  sync_root_tracker->set_tracker_id(tracker_id);
  sync_root_tracker->set_file_id(sync_root_metadata.file_id());
  sync_root_tracker->set_parent_tracker_id(0);
  sync_root_tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  sync_root_tracker->set_dirty(false);
  sync_root_tracker->set_active(true);
  sync_root_tracker->set_needs_folder_listing(false);
  *sync_root_tracker->mutable_synced_details() = sync_root_metadata.details();
  return sync_root_tracker.Pass();
}

scoped_ptr<FileTracker> CreateInitialAppRootTracker(
    int64 tracker_id,
    int64 parent_tracker_id,
    const FileMetadata& app_root_metadata) {
  scoped_ptr<FileTracker> app_root_tracker(new FileTracker);
  app_root_tracker->set_tracker_id(tracker_id);
  app_root_tracker->set_parent_tracker_id(parent_tracker_id);
  app_root_tracker->set_file_id(app_root_metadata.file_id());
  app_root_tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  app_root_tracker->set_dirty(false);
  app_root_tracker->set_active(false);
  app_root_tracker->set_needs_folder_listing(false);
  *app_root_tracker->mutable_synced_details() = app_root_metadata.details();
  return app_root_tracker.Pass();
}

void AdaptLevelDBStatusToSyncStatusCode(const SyncStatusCallback& callback,
                                        const leveldb::Status& status) {
  callback.Run(LevelDBStatusToSyncStatusCode(status));
}

void PutFileDeletionToBatch(const std::string& file_id,
                            leveldb::WriteBatch* batch) {
  if (batch)
    batch->Delete(kFileMetadataKeyPrefix + file_id);
}

void PutTrackerDeletionToBatch(int64 tracker_id, leveldb::WriteBatch* batch) {
  if (batch)
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
                            leveldb::Env* env_override,
                            scoped_ptr<leveldb::DB>* db_out,
                            bool* created) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db_out);
  DCHECK(created);

  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  if (env_override)
    options.env = env_override;
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
    if (batch)
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

bool HasInvalidTitle(const std::string& title) {
  return title.find('/') != std::string::npos ||
      title.find('\\') != std::string::npos;
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
                              leveldb::Env* env_override,
                              const CreateCallback& callback) {
  task_runner->PostTask(FROM_HERE, base::Bind(
      &CreateOnTaskRunner,
      base::MessageLoopProxy::current(),
      make_scoped_refptr(task_runner),
      database_path, env_override, callback));
}

MetadataDatabase::~MetadataDatabase() {
  task_runner_->DeleteSoon(FROM_HERE, db_.release());
  STLDeleteContainerPairSecondPointers(
      file_by_id_.begin(), file_by_id_.end());
  STLDeleteContainerPairSecondPointers(
      tracker_by_id_.begin(), tracker_by_id_.end());
}

// static
void MetadataDatabase::ClearDatabase(
    scoped_ptr<MetadataDatabase> metadata_database) {
  DCHECK(metadata_database);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      metadata_database->task_runner_;
  base::FilePath database_path = metadata_database->database_path_;
  DCHECK(!database_path.empty());
  metadata_database.reset();

  task_runner->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(base::DeleteFile),
                 database_path, true /* recursive */));
}

int64 MetadataDatabase::GetLargestFetchedChangeID() const {
  return service_metadata_->largest_change_id();
}

int64 MetadataDatabase::GetSyncRootTrackerID() const {
  return service_metadata_->sync_root_tracker_id();
}

int64 MetadataDatabase::GetLargestKnownChangeID() const {
  DCHECK_LE(GetLargestFetchedChangeID(), largest_known_change_id_);
  return largest_known_change_id_;
}

void MetadataDatabase::UpdateLargestKnownChangeID(int64 change_id) {
  if (largest_known_change_id_ < change_id)
    largest_known_change_id_ = change_id;
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
  UpdateLargestKnownChangeID(largest_change_id);

  AttachSyncRoot(sync_root_folder, batch.get());
  for (size_t i = 0; i < app_root_folders.size(); ++i)
    AttachInitialAppRoot(*app_root_folders[i], batch.get());

  WriteToDatabase(batch.Pass(), callback);
}

bool MetadataDatabase::IsAppEnabled(const std::string& app_id) const {
  FileTracker tracker;
  if (!FindAppRootTracker(app_id, &tracker))
    return false;
  return tracker.tracker_kind() == TRACKER_KIND_APP_ROOT;
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

base::FilePath MetadataDatabase::BuildDisplayPathForTracker(
    const FileTracker& tracker) const {
  base::FilePath path;
  if (tracker.active()) {
    BuildPathForTracker(tracker.tracker_id(), &path);
    return path;
  }
  BuildPathForTracker(tracker.parent_tracker_id(), &path);
  if (tracker.has_synced_details()) {
    path = path.Append(
        base::FilePath::FromUTF8Unsafe(tracker.synced_details().title()));
  } else {
    path = path.Append(FILE_PATH_LITERAL("<unknown>"));
  }
  return path;
}

bool MetadataDatabase::FindNearestActiveAncestor(
    const std::string& app_id,
    const base::FilePath& full_path,
    FileTracker* tracker,
    base::FilePath* path) const {
  DCHECK(tracker);
  DCHECK(path);

  if (full_path.IsAbsolute() ||
      !FindAppRootTracker(app_id, tracker) ||
      tracker->tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT) {
    return false;
  }

  std::vector<base::FilePath::StringType> components;
  full_path.GetComponents(&components);
  path->clear();

  for (size_t i = 0; i < components.size(); ++i) {
    const std::string title = base::FilePath(components[i]).AsUTF8Unsafe();
    TrackerSet trackers;
    if (!FindTrackersByParentAndTitle(
            tracker->tracker_id(), title, &trackers) ||
        !trackers.has_active()) {
      return true;
    }

    DCHECK(trackers.active_tracker()->has_synced_details());
    const FileDetails& details = trackers.active_tracker()->synced_details();
    if (details.file_kind() != FILE_KIND_FOLDER && i != components.size() - 1) {
      // This non-last component indicates file. Give up search.
      return true;
    }

    *tracker = *trackers.active_tracker();
    *path = path->Append(components[i]);
  }

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
    if (HasNewerFileMetadata(change.file_id(), change.change_id()))
      continue;

    scoped_ptr<FileMetadata> file(
        CreateFileMetadataFromChangeResource(change));
    UpdateByFileMetadata(FROM_HERE, file.Pass(),
                         UPDATE_TRACKER_FOR_UNSYNCED_FILE,
                         batch.get());
  }

  UpdateLargestKnownChangeID(largest_change_id);
  service_metadata_->set_largest_change_id(largest_change_id);
  PutServiceMetadataToBatch(*service_metadata_, batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::UpdateByFileResource(
    const google_apis::FileResource& resource,
    const SyncStatusCallback& callback) {
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);

  scoped_ptr<FileMetadata> file(
      CreateFileMetadataFromFileResource(
          GetLargestKnownChangeID(), resource));
  UpdateByFileMetadata(FROM_HERE, file.Pass(),
                       UPDATE_TRACKER_FOR_UNSYNCED_FILE,
                       batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::UpdateByFileResourceList(
    ScopedVector<google_apis::FileResource> resources,
    const SyncStatusCallback& callback) {
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);

  for (size_t i = 0; i < resources.size(); ++i) {
    scoped_ptr<FileMetadata> file(
        CreateFileMetadataFromFileResource(
            GetLargestKnownChangeID(), *resources[i]));
    UpdateByFileMetadata(FROM_HERE, file.Pass(),
                         UPDATE_TRACKER_FOR_UNSYNCED_FILE,
                         batch.get());
  }
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::UpdateByDeletedRemoteFile(
    const std::string& file_id,
    const SyncStatusCallback& callback) {
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  scoped_ptr<FileMetadata> file(
      CreateDeletedFileMetadata(GetLargestKnownChangeID(), file_id));
  UpdateByFileMetadata(FROM_HERE, file.Pass(),
                       UPDATE_TRACKER_FOR_UNSYNCED_FILE,
                       batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::UpdateByDeletedRemoteFileList(
    const FileIDList& file_ids,
    const SyncStatusCallback& callback) {
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  for (FileIDList::const_iterator itr = file_ids.begin();
       itr != file_ids.end(); ++itr) {
    scoped_ptr<FileMetadata> file(
        CreateDeletedFileMetadata(GetLargestKnownChangeID(), *itr));
    UpdateByFileMetadata(FROM_HERE, file.Pass(),
                         UPDATE_TRACKER_FOR_UNSYNCED_FILE,
                         batch.get());
  }
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::ReplaceActiveTrackerWithNewResource(
    int64 parent_tracker_id,
    const google_apis::FileResource& resource,
    const SyncStatusCallback& callback) {
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);

  DCHECK(!ContainsKey(file_by_id_, resource.file_id()));
  UpdateByFileMetadata(
      FROM_HERE,
      CreateFileMetadataFromFileResource(GetLargestKnownChangeID(), resource),
      UPDATE_TRACKER_FOR_SYNCED_FILE,
      batch.get());
  DCHECK(ContainsKey(file_by_id_, resource.file_id()));

  ForceActivateTrackerByPath(
      parent_tracker_id, resource.title(), resource.file_id(), batch.get());

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
  if (folder_tracker->dirty() && !ShouldKeepDirty(*folder_tracker))
    ClearDirty(folder_tracker, NULL);
  PutFileTrackerToBatch(*folder_tracker, batch.get());

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

  if (updated_details.missing()) {
    FileByID::iterator found = file_by_id_.find(tracker->file_id());
    if (found == file_by_id_.end() || found->second->details().missing()) {
      // Both the tracker and metadata have the missing flag, now it's safe to
      // delete the |tracker|.
      RemoveTracker(tracker->tracker_id(), batch.get());
      WriteToDatabase(batch.Pass(), callback);
      return;
    }
  }

  // Sync-root deletion should be handled separately by SyncEngine.
  DCHECK(tracker_id != GetSyncRootTrackerID() ||
         (tracker->has_synced_details() &&
          tracker->synced_details().title() == updated_details.title() &&
          !updated_details.missing()));

  if (tracker_id != GetSyncRootTrackerID()) {
    // Check if the tracker's parent is still in |parent_tracker_ids|.
    // If not, there should exist another tracker for the new parent, so delete
    // old tracker.
    DCHECK(ContainsKey(tracker_by_id_, tracker->parent_tracker_id()));
    FileTracker* parent_tracker = tracker_by_id_[tracker->parent_tracker_id()];
    if (!HasFileAsParent(updated_details, parent_tracker->file_id())) {
      RemoveTracker(tracker->tracker_id(), batch.get());
      WriteToDatabase(batch.Pass(), callback);
      return;
    }

    if (tracker->has_synced_details()) {
      // Check if the tracker was retitled.  If it was, there should exist
      // another tracker for the new title, so delete old tracker.
      if (tracker->synced_details().title() != updated_details.title()) {
        RemoveTracker(tracker->tracker_id(), batch.get());
        WriteToDatabase(batch.Pass(), callback);
        return;
      }
    } else {
      int64 parent_tracker_id = parent_tracker->tracker_id();
      const std::string& title = updated_details.title();
      TrackerSet* trackers =
          &trackers_by_parent_and_title_[parent_tracker_id][title];

      for (TrackerSet::iterator itr = trackers->begin();
           itr != trackers->end(); ++itr) {
        if ((*itr)->file_id() == tracker->file_id()) {
          RemoveTracker(tracker->tracker_id(), batch.get());
          WriteToDatabase(batch.Pass(), callback);
          return;
        }
      }

      trackers_by_parent_and_title_[parent_tracker_id][std::string()].Erase(
          tracker);
      trackers->Insert(tracker);
    }
  }

  *tracker->mutable_synced_details() = updated_details;

  // Activate the tracker if:
  //   - There is no active tracker that tracks |tracker->file_id()|.
  //   - There is no active tracker that has the same |parent| and |title|.
  if (!tracker->active() && CanActivateTracker(*tracker))
    MakeTrackerActive(tracker->tracker_id(), batch.get());
  if (tracker->dirty() && !ShouldKeepDirty(*tracker))
    ClearDirty(tracker, NULL);
  PutFileTrackerToBatch(*tracker, batch.get());

  WriteToDatabase(batch.Pass(), callback);
}

MetadataDatabase::ActivationStatus MetadataDatabase::TryActivateTracker(
    int64 parent_tracker_id,
    const std::string& file_id,
    const SyncStatusCallback& callback) {
  DCHECK(ContainsKey(tracker_by_id_, parent_tracker_id));
  DCHECK(ContainsKey(file_by_id_, file_id));

  FileMetadata file;
  if (!FindFileByFileID(file_id, &file)) {
    NOTREACHED();
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_FAILED));
    return ACTIVATION_PENDING;
  }
  std::string title = file.details().title();
  DCHECK(!HasInvalidTitle(title));

  TrackerSet same_file_id;
  FindTrackersByFileID(file_id, &same_file_id);

  // Pick up the tracker to be activated, that has:
  //  - |parent_tarcker_id| as the parent, and
  //  - |file_id| as the tracker's |file_id|.
  FileTracker* tracker = NULL;
  for (TrackerSet::iterator itr = same_file_id.begin();
       itr != same_file_id.end(); ++itr) {
    FileTracker* candidate = *itr;
    if (candidate->parent_tracker_id() != parent_tracker_id)
      continue;

    if (candidate->has_synced_details() &&
        candidate->synced_details().title() != title)
      continue;
    tracker = candidate;
  }

  DCHECK(tracker);

  // Check if there is another active tracker that tracks |file_id|.
  // This can happen when the tracked file has multiple parents.
  // If this case, report the failure to the caller.
  if (!tracker->active() && same_file_id.has_active())
    return ACTIVATION_FAILED_ANOTHER_ACTIVE_TRACKER;

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);

  if (!tracker->active()) {
    // Check if there is another active tracker that has the same path to
    // the tracker to be activated.
    // Assuming the caller already overrides local file with newly added file,
    // inactivate existing active tracker.
    TrackerSet same_title;
    FindTrackersByParentAndTitle(parent_tracker_id, title, &same_title);
    if (same_title.has_active()) {
      MakeTrackerInactive(same_title.active_tracker()->tracker_id(),
                          batch.get());
    }
  }

  if (!tracker->has_synced_details() ||
      tracker->synced_details().title() != title) {
    trackers_by_parent_and_title_[parent_tracker_id]
        [GetTrackerTitle(*tracker)].Erase(tracker);
    trackers_by_parent_and_title_[parent_tracker_id][title].Insert(
        tracker);
  }
  *tracker->mutable_synced_details() = file.details();

  MakeTrackerActive(tracker->tracker_id(), batch.get());
  ClearDirty(tracker, batch.get());

  WriteToDatabase(batch.Pass(), callback);
  return ACTIVATION_PENDING;
}

void MetadataDatabase::LowerTrackerPriority(int64 tracker_id) {
  TrackerByID::const_iterator found = tracker_by_id_.find(tracker_id);
  if (found == tracker_by_id_.end())
    return;

  FileTracker* tracker = found->second;
  if (dirty_trackers_.erase(tracker))
    low_priority_dirty_trackers_.insert(tracker);
}

void MetadataDatabase::PromoteLowerPriorityTrackersToNormal() {
  if (dirty_trackers_.empty()) {
    dirty_trackers_.swap(low_priority_dirty_trackers_);
    return;
  }
  dirty_trackers_.insert(low_priority_dirty_trackers_.begin(),
                         low_priority_dirty_trackers_.end());
  low_priority_dirty_trackers_.clear();
}

bool MetadataDatabase::GetNormalPriorityDirtyTracker(
    FileTracker* tracker) const {
  DirtyTrackers::const_iterator itr = dirty_trackers_.begin();
  if (itr == dirty_trackers_.end())
    return false;
  if (tracker)
    *tracker = **itr;
  return true;
}

bool MetadataDatabase::GetLowPriorityDirtyTracker(
    FileTracker* tracker) const {
  DirtyTrackers::const_iterator itr = low_priority_dirty_trackers_.begin();
  if (itr == low_priority_dirty_trackers_.end())
    return false;
  if (tracker)
    *tracker = **itr;
  return true;
}

bool MetadataDatabase::GetMultiParentFileTrackers(std::string* file_id,
                                                  TrackerSet* trackers) {
  DCHECK(file_id);
  DCHECK(trackers);
  // TODO(tzik): Make this function more efficient.
  for (TrackersByFileID::const_iterator itr = trackers_by_file_id_.begin();
       itr != trackers_by_file_id_.end(); ++itr) {
    if (itr->second.size() > 1 && itr->second.has_active()) {
      *file_id = itr->first;
      *trackers = itr->second;
      return true;
    }
  }
  return false;
}

bool MetadataDatabase::GetConflictingTrackers(TrackerSet* trackers) {
  DCHECK(trackers);
  // TODO(tzik): Make this function more efficient.
  for (TrackersByParentAndTitle::const_iterator parent_itr =
           trackers_by_parent_and_title_.begin();
       parent_itr != trackers_by_parent_and_title_.end();
       ++parent_itr) {
    const TrackersByTitle& trackers_by_title = parent_itr->second;
    for (TrackersByTitle::const_iterator itr = trackers_by_title.begin();
         itr != trackers_by_title.end();
         ++itr) {
      if (itr->second.size() > 1 && itr->second.has_active()) {
        *trackers = itr->second;
        return true;
      }
    }
  }
  return false;
}

void MetadataDatabase::GetRegisteredAppIDs(std::vector<std::string>* app_ids) {
  DCHECK(app_ids);
  app_ids->clear();
  app_ids->reserve(app_root_by_app_id_.size());
  for (TrackerByAppID::iterator itr = app_root_by_app_id_.begin();
       itr != app_root_by_app_id_.end(); ++itr) {
    app_ids->push_back(itr->first);
  }
}

MetadataDatabase::MetadataDatabase(base::SequencedTaskRunner* task_runner,
                                   const base::FilePath& database_path,
                                   leveldb::Env* env_override)
    : task_runner_(task_runner),
      database_path_(database_path),
      env_override_(env_override),
      largest_known_change_id_(0),
      weak_ptr_factory_(this) {
  DCHECK(task_runner);
}

// static
void MetadataDatabase::CreateOnTaskRunner(
    base::SingleThreadTaskRunner* callback_runner,
    base::SequencedTaskRunner* task_runner,
    const base::FilePath& database_path,
    leveldb::Env* env_override,
    const CreateCallback& callback) {
  scoped_ptr<MetadataDatabase> metadata_database(
      new MetadataDatabase(task_runner, database_path, env_override));
  SyncStatusCode status =
      metadata_database->InitializeOnTaskRunner();
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
      new MetadataDatabase(base::MessageLoopProxy::current(),
                           base::FilePath(), NULL));
  metadata_database->db_ = db.Pass();
  SyncStatusCode status =
      metadata_database->InitializeOnTaskRunner();
  if (status == SYNC_STATUS_OK)
    *metadata_database_out = metadata_database.Pass();
  return status;
}

SyncStatusCode MetadataDatabase::InitializeOnTaskRunner() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  SyncStatusCode status = SYNC_STATUS_UNKNOWN;
  bool created = false;
  // Open database unless |db_| is overridden for testing.
  if (!db_) {
    status = OpenDatabase(database_path_, env_override_, &db_, &created);
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
  UpdateLargestKnownChangeID(service_metadata_->largest_change_id());

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

  // Make |tracker| a normal priority dirty tracker.
  MarkSingleTrackerAsDirty(tracker, NULL);
  PutFileTrackerToBatch(*tracker, batch);
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
  PutFileTrackerToBatch(*tracker, batch);
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
  PutFileTrackerToBatch(*tracker, batch);
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
  PutFileTrackerToBatch(*tracker, batch);
}

void MetadataDatabase::CreateTrackerForParentAndFileID(
    const FileTracker& parent_tracker,
    const std::string& file_id,
    leveldb::WriteBatch* batch) {
  CreateTrackerInternal(parent_tracker, file_id, NULL,
                        UPDATE_TRACKER_FOR_UNSYNCED_FILE,
                        batch);
}

void MetadataDatabase::CreateTrackerForParentAndFileMetadata(
    const FileTracker& parent_tracker,
    const FileMetadata& file_metadata,
    UpdateOption option,
    leveldb::WriteBatch* batch) {
  DCHECK(file_metadata.has_details());
  CreateTrackerInternal(parent_tracker,
                        file_metadata.file_id(),
                        &file_metadata.details(),
                        option,
                        batch);
}

void MetadataDatabase::CreateTrackerInternal(const FileTracker& parent_tracker,
                                             const std::string& file_id,
                                             const FileDetails* details,
                                             UpdateOption option,
                                             leveldb::WriteBatch* batch) {
  int64 tracker_id = IncrementTrackerID(batch);
  scoped_ptr<FileTracker> tracker(new FileTracker);
  tracker->set_tracker_id(tracker_id);
  tracker->set_parent_tracker_id(parent_tracker.tracker_id());
  tracker->set_file_id(file_id);
  tracker->set_app_id(parent_tracker.app_id());
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  tracker->set_dirty(true);
  tracker->set_active(false);
  tracker->set_needs_folder_listing(false);
  if (details) {
    *tracker->mutable_synced_details() = *details;
    if (option == UPDATE_TRACKER_FOR_UNSYNCED_FILE) {
      tracker->mutable_synced_details()->set_missing(true);
      tracker->mutable_synced_details()->clear_md5();
    }
  }
  PutFileTrackerToBatch(*tracker, batch);

  trackers_by_file_id_[file_id].Insert(tracker.get());
  // Note: |trackers_by_parent_and_title_| does not map from
  // FileMetadata::details but from FileTracker::synced_details, which is filled
  // on tracker updated phase.  Use empty string as the title since
  // FileTracker::synced_details is empty here.
  std::string title;
  if (details)
    title = details->title();
  trackers_by_parent_and_title_[parent_tracker.tracker_id()][title]
      .Insert(tracker.get());
  dirty_trackers_.insert(tracker.get());
  StoreFileTracker(tracker.Pass());
}

void MetadataDatabase::RemoveTracker(int64 tracker_id,
                                     leveldb::WriteBatch* batch) {
  RemoveTrackerInternal(tracker_id, batch, false);
  RemoveAllDescendantTrackers(tracker_id, batch);
}

void MetadataDatabase::RemoveTrackerIgnoringSameTitle(
    int64 tracker_id,
    leveldb::WriteBatch* batch) {
  RemoveTrackerInternal(tracker_id, batch, true);
}

void MetadataDatabase::RemoveTrackerInternal(
    int64 tracker_id,
    leveldb::WriteBatch* batch,
    bool ignoring_same_title) {
  scoped_ptr<FileTracker> tracker(
      FindAndEraseItem(&tracker_by_id_, tracker_id));
  if (!tracker)
    return;

  EraseTrackerFromFileIDIndex(tracker.get(), batch);
  if (IsAppRoot(*tracker))
    app_root_by_app_id_.erase(tracker->app_id());
  EraseTrackerFromPathIndex(tracker.get());
  dirty_trackers_.erase(tracker.get());
  low_priority_dirty_trackers_.erase(tracker.get());

  MarkTrackersDirtyByFileID(tracker->file_id(), batch);
  if (!ignoring_same_title) {
    // Mark trackers having the same title with the given tracker as dirty.
    MarkTrackersDirtyByPath(tracker->parent_tracker_id(),
                            GetTrackerTitle(*tracker),
                            batch);
  }
  PutTrackerDeletionToBatch(tracker_id, batch);
}

void MetadataDatabase::MaybeAddTrackersForNewFile(
    const FileMetadata& file,
    UpdateOption option,
    leveldb::WriteBatch* batch) {
  std::set<int64> parents_to_exclude;
  TrackersByFileID::iterator found = trackers_by_file_id_.find(file.file_id());
  if (found != trackers_by_file_id_.end()) {
    for (TrackerSet::const_iterator itr = found->second.begin();
         itr != found->second.end(); ++itr) {
      const FileTracker& tracker = **itr;
      int64 parent_tracker_id = tracker.parent_tracker_id();
      if (!parent_tracker_id)
        continue;

      // Exclude |parent_tracker_id| if it already has a tracker that has
      // unknown title or has the same title with |file|.
      if (!tracker.has_synced_details() ||
          tracker.synced_details().title() == file.details().title()) {
        parents_to_exclude.insert(parent_tracker_id);
      }
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

      if (ContainsKey(parents_to_exclude, parent_tracker_id))
        continue;

      CreateTrackerForParentAndFileMetadata(
          *parent_tracker, file, option, batch);
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
    RemoveTrackerIgnoringSameTitle(tracker_id, batch);
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

void MetadataDatabase::MarkSingleTrackerAsDirty(FileTracker* tracker,
                                                leveldb::WriteBatch* batch) {
  if (!tracker->dirty()) {
    tracker->set_dirty(true);
    PutFileTrackerToBatch(*tracker, batch);
  }
  dirty_trackers_.insert(tracker);
  low_priority_dirty_trackers_.erase(tracker);
}

void MetadataDatabase::ClearDirty(FileTracker* tracker,
                                  leveldb::WriteBatch* batch) {
  if (tracker->dirty()) {
    tracker->set_dirty(false);
    PutFileTrackerToBatch(*tracker, batch);
  }

  dirty_trackers_.erase(tracker);
  low_priority_dirty_trackers_.erase(tracker);
}

void MetadataDatabase::MarkTrackerSetDirty(
    TrackerSet* trackers,
    leveldb::WriteBatch* batch) {
  for (TrackerSet::iterator itr = trackers->begin();
       itr != trackers->end(); ++itr) {
    MarkSingleTrackerAsDirty(*itr, batch);
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
  if (found == trackers_by_parent_and_title_.end())
    return;

  TrackersByTitle::iterator itr = found->second.find(title);
  if (itr != found->second.end())
    MarkTrackerSetDirty(&itr->second, batch);
}

int64 MetadataDatabase::IncrementTrackerID(leveldb::WriteBatch* batch) {
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

    MarkSingleTrackerAsDirty(tracker_by_id_[tracker_id], batch);
  }
}

bool MetadataDatabase::CanActivateTracker(const FileTracker& tracker) {
  DCHECK(!tracker.active());
  DCHECK_NE(service_metadata_->sync_root_tracker_id(), tracker.tracker_id());

  if (HasActiveTrackerForFileID(tracker.file_id()))
    return false;

  if (tracker.app_id().empty() &&
      tracker.tracker_id() != GetSyncRootTrackerID()) {
    return false;
  }

  if (!tracker.has_synced_details())
    return false;
  if (tracker.synced_details().file_kind() == FILE_KIND_UNSUPPORTED)
    return false;
  if (HasInvalidTitle(tracker.synced_details().title()))
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
  DCHECK(file->has_details());

  const FileDetails& local_details = tracker.synced_details();
  const FileDetails& remote_details = file->details();

  if (tracker.active()) {
    if (tracker.needs_folder_listing())
      return true;
    if (tracker.synced_details().md5() != file->details().md5())
      return true;
    if (local_details.missing() != remote_details.missing())
      return true;
  }

  if (local_details.title() != remote_details.title())
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

void MetadataDatabase::RemoveUnneededTrackersForMissingFile(
    const std::string& file_id,
    leveldb::WriteBatch* batch) {
  TrackerSet trackers;
  FindTrackersByFileID(file_id, &trackers);
  for (TrackerSet::const_iterator itr = trackers.begin();
       itr != trackers.end(); ++itr) {
    const FileTracker& tracker = **itr;
    if (!tracker.has_synced_details() ||
        tracker.synced_details().missing()) {
      RemoveTracker(tracker.tracker_id(), batch);
    }
  }
}

void MetadataDatabase::UpdateByFileMetadata(
    const tracked_objects::Location& from_where,
    scoped_ptr<FileMetadata> file,
    UpdateOption option,
    leveldb::WriteBatch* batch) {
  DCHECK(file);
  DCHECK(file->has_details());

  DVLOG(1) << from_where.function_name() << ": "
           << file->file_id() << " ("
           << file->details().title() << ")"
           << (file->details().missing() ? " deleted" : "");

  std::string file_id = file->file_id();
  if (file->details().missing())
    RemoveUnneededTrackersForMissingFile(file_id, batch);
  else
    MaybeAddTrackersForNewFile(*file, option, batch);

  if (FindTrackersByFileID(file_id, NULL)) {
    if (option != UPDATE_TRACKER_FOR_SYNCED_FILE)
      MarkTrackersDirtyByFileID(file_id, batch);
    PutFileMetadataToBatch(*file, batch);
    StoreFileMetadata(file.Pass());
  }
}

void MetadataDatabase::WriteToDatabase(scoped_ptr<leveldb::WriteBatch> batch,
                                       const SyncStatusCallback& callback) {
  if (!batch) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

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
    base::DictionaryValue* file = new base::DictionaryValue;

    base::FilePath path = BuildDisplayPathForTracker(*tracker);
    file->SetString("path", path.AsUTF8Unsafe());
    if (tracker->has_synced_details()) {
      file->SetString("title", tracker->synced_details().title());
      file->SetString("type",
                      FileKindToString(tracker->synced_details().file_kind()));
    }

    base::DictionaryValue* details = new base::DictionaryValue;
    details->SetString("file_id", tracker->file_id());
    if (tracker->has_synced_details() &&
        tracker->synced_details().file_kind() == FILE_KIND_FILE)
      details->SetString("md5", tracker->synced_details().md5());
    details->SetString("active", tracker->active() ? "true" : "false");
    details->SetString("dirty", tracker->dirty() ? "true" : "false");

    file->Set("details", details);

    files->Append(file);
  }

  return files.Pass();
}

scoped_ptr<base::ListValue> MetadataDatabase::DumpDatabase() {
  scoped_ptr<base::ListValue> list(new base::ListValue);
  list->Append(DumpTrackers().release());
  list->Append(DumpMetadata().release());
  return list.Pass();
}

bool MetadataDatabase::HasNewerFileMetadata(const std::string& file_id,
                                            int64 change_id) {
  FileByID::const_iterator found = file_by_id_.find(file_id);
  if (found == file_by_id_.end())
    return false;
  DCHECK(found->second->has_details());
  return found->second->details().change_id() >= change_id;
}

scoped_ptr<base::ListValue> MetadataDatabase::DumpTrackers() {
  scoped_ptr<base::ListValue> trackers(new base::ListValue);

  // Append the first element for metadata.
  base::DictionaryValue* metadata = new base::DictionaryValue;
  const char *trackerKeys[] = {
    "tracker_id", "path", "file_id", "tracker_kind", "app_id",
    "active", "dirty", "folder_listing",
    "title", "kind", "md5", "etag", "missing", "change_id",
  };
  std::vector<std::string> key_strings(
      trackerKeys, trackerKeys + ARRAYSIZE_UNSAFE(trackerKeys));
  base::ListValue* keys = new base::ListValue;
  keys->AppendStrings(key_strings);
  metadata->SetString("title", "Trackers");
  metadata->Set("keys", keys);
  trackers->Append(metadata);

  // Append tracker data.
  for (TrackerByID::const_iterator itr = tracker_by_id_.begin();
       itr != tracker_by_id_.end(); ++itr) {
    const FileTracker& tracker = *itr->second;
    base::DictionaryValue* dict = new base::DictionaryValue;
    base::FilePath path = BuildDisplayPathForTracker(tracker);
    dict->SetString("tracker_id", base::Int64ToString(tracker.tracker_id()));
    dict->SetString("path", path.AsUTF8Unsafe());
    dict->SetString("file_id", tracker.file_id());
    TrackerKind tracker_kind = tracker.tracker_kind();
    dict->SetString(
        "tracker_kind",
        tracker_kind == TRACKER_KIND_APP_ROOT ? "AppRoot" :
        tracker_kind == TRACKER_KIND_DISABLED_APP_ROOT ? "Disabled App" :
        tracker.tracker_id() == GetSyncRootTrackerID() ? "SyncRoot" :
        "Regular");
    dict->SetString("app_id", tracker.app_id());
    dict->SetString("active", tracker.active() ? "true" : "false");
    dict->SetString("dirty", tracker.dirty() ? "true" : "false");
    dict->SetString("folder_listing",
                    tracker.needs_folder_listing() ? "needed" : "no");
    if (tracker.has_synced_details()) {
      const FileDetails& details = tracker.synced_details();
      dict->SetString("title", details.title());
      dict->SetString("kind", FileKindToString(details.file_kind()));
      dict->SetString("md5", details.md5());
      dict->SetString("etag", details.etag());
      dict->SetString("missing", details.missing() ? "true" : "false");
      dict->SetString("change_id", base::Int64ToString(details.change_id()));
    }
    trackers->Append(dict);
  }
  return trackers.Pass();
}

scoped_ptr<base::ListValue> MetadataDatabase::DumpMetadata() {
  scoped_ptr<base::ListValue> files(new base::ListValue);

  // Append the first element for metadata.
  base::DictionaryValue* metadata = new base::DictionaryValue;
  const char *fileKeys[] = {
    "file_id", "title", "type", "md5", "etag", "missing",
    "change_id", "parents"
  };
  std::vector<std::string> key_strings(
      fileKeys, fileKeys + ARRAYSIZE_UNSAFE(fileKeys));
  base::ListValue* keys = new base::ListValue;
  keys->AppendStrings(key_strings);
  metadata->SetString("title", "Metadata");
  metadata->Set("keys", keys);
  files->Append(metadata);

  // Append metadata data.
  for (FileByID::const_iterator itr = file_by_id_.begin();
       itr != file_by_id_.end(); ++itr) {
    const FileMetadata& file = *itr->second;

    base::DictionaryValue* dict = new base::DictionaryValue;
    dict->SetString("file_id", file.file_id());
    if (file.has_details()) {
      const FileDetails& details = file.details();
      dict->SetString("title", details.title());
      dict->SetString("type", FileKindToString(details.file_kind()));
      dict->SetString("md5", details.md5());
      dict->SetString("etag", details.etag());
      dict->SetString("missing", details.missing() ? "true" : "false");
      dict->SetString("change_id", base::Int64ToString(details.change_id()));

      std::vector<std::string> parents;
      for (int i = 0; i < details.parent_folder_ids_size(); ++i)
        parents.push_back(details.parent_folder_ids(i));
      dict->SetString("parents", JoinString(parents, ","));
    }
    files->Append(dict);
  }
  return files.Pass();
}

void MetadataDatabase::StoreFileMetadata(
    scoped_ptr<FileMetadata> file_metadata) {
  DCHECK(file_metadata);

  std::string file_id = file_metadata->file_id();
  FileMetadata* file_metadata_ptr = file_metadata.release();
  std::swap(file_metadata_ptr, file_by_id_[file_id]);
  delete file_metadata_ptr;
}

void MetadataDatabase::StoreFileTracker(scoped_ptr<FileTracker> file_tracker) {
  DCHECK(file_tracker);

  int64 tracker_id = file_tracker->tracker_id();
  DCHECK(!ContainsKey(tracker_by_id_, tracker_id));
  tracker_by_id_[tracker_id] = file_tracker.release();
}

void MetadataDatabase::AttachSyncRoot(
    const google_apis::FileResource& sync_root_folder,
    leveldb::WriteBatch* batch) {
  scoped_ptr<FileMetadata> sync_root_metadata =
      CreateFileMetadataFromFileResource(
          GetLargestKnownChangeID(), sync_root_folder);
  scoped_ptr<FileTracker> sync_root_tracker =
      CreateSyncRootTracker(IncrementTrackerID(batch), *sync_root_metadata);

  PutFileMetadataToBatch(*sync_root_metadata, batch);
  PutFileTrackerToBatch(*sync_root_tracker, batch);

  service_metadata_->set_sync_root_tracker_id(sync_root_tracker->tracker_id());
  PutServiceMetadataToBatch(*service_metadata_, batch);

  InsertFileTrackerToIndex(sync_root_tracker.get());

  StoreFileMetadata(sync_root_metadata.Pass());
  StoreFileTracker(sync_root_tracker.Pass());
}

void MetadataDatabase::AttachInitialAppRoot(
    const google_apis::FileResource& app_root_folder,
    leveldb::WriteBatch* batch) {
  scoped_ptr<FileMetadata> app_root_metadata =
      CreateFileMetadataFromFileResource(
          GetLargestKnownChangeID(), app_root_folder);
  scoped_ptr<FileTracker> app_root_tracker =
      CreateInitialAppRootTracker(IncrementTrackerID(batch),
                                  GetSyncRootTrackerID(),
                                  *app_root_metadata);

  PutFileMetadataToBatch(*app_root_metadata, batch);
  PutFileTrackerToBatch(*app_root_tracker, batch);

  InsertFileTrackerToIndex(app_root_tracker.get());

  StoreFileMetadata(app_root_metadata.Pass());
  StoreFileTracker(app_root_tracker.Pass());
}

void MetadataDatabase::InsertFileTrackerToIndex(FileTracker* tracker) {
  trackers_by_file_id_[tracker->file_id()].Insert(tracker);
  if (IsAppRoot(*tracker)) {
    DCHECK(!ContainsKey(app_root_by_app_id_, tracker->app_id()));
    app_root_by_app_id_[tracker->app_id()] = tracker;
  }

  int64 parent_tracker_id = tracker->parent_tracker_id();
  if (parent_tracker_id) {
    std::string title = GetTrackerTitle(*tracker);
    trackers_by_parent_and_title_[parent_tracker_id][title].Insert(tracker);
  }

  if (tracker->dirty()) {
    dirty_trackers_.insert(tracker);
    low_priority_dirty_trackers_.erase(tracker);
  }
}

void MetadataDatabase::ForceActivateTrackerByPath(int64 parent_tracker_id,
                                                  const std::string& title,
                                                  const std::string& file_id,
                                                  leveldb::WriteBatch* batch) {
  DCHECK(ContainsKey(trackers_by_parent_and_title_, parent_tracker_id));
  DCHECK(ContainsKey(trackers_by_parent_and_title_[parent_tracker_id], title));
  DCHECK(!trackers_by_file_id_[file_id].has_active());

  TrackerSet* same_path_trackers =
      &trackers_by_parent_and_title_[parent_tracker_id][title];

  for (TrackerSet::iterator itr = same_path_trackers->begin();
       itr != same_path_trackers->end(); ++itr) {
    FileTracker* tracker = *itr;
    if (tracker->file_id() != file_id)
      continue;

    if (same_path_trackers->has_active()) {
      MakeTrackerInactive(
          same_path_trackers->active_tracker()->tracker_id(), batch);
    }
    MakeTrackerActive(tracker->tracker_id(), batch);
    ClearDirty(tracker, batch);
    return;
  }

  NOTREACHED();
}

}  // namespace drive_backend
}  // namespace sync_file_system
