// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

#include <algorithm>
#include <stack>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_vector.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/leveldb_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_on_disk.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

// Command line flag to enable on-disk indexing.
const char kEnableMetadataDatabaseOnDisk[] = "enable-syncfs-on-disk-indexing";

void EmptyStatusCallback(SyncStatusCode status) {}

std::string FileKindToString(FileKind file_kind) {
  switch (file_kind) {
    case FILE_KIND_UNSUPPORTED:
      return "unsupported";
    case FILE_KIND_FILE:
      return "file";
    case FILE_KIND_FOLDER:
      return "folder";
  }

  NOTREACHED();
  return "unknown";
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

void PopulateFileDetailsByFileResource(
    const google_apis::FileResource& file_resource,
    FileDetails* details) {
  details->clear_parent_folder_ids();
  for (std::vector<google_apis::ParentReference>::const_iterator itr =
           file_resource.parents().begin();
       itr != file_resource.parents().end();
       ++itr) {
    details->add_parent_folder_ids(itr->file_id());
  }
  details->set_title(file_resource.title());

  if (file_resource.IsDirectory())
    details->set_file_kind(FILE_KIND_FOLDER);
  else if (file_resource.IsHostedDocument())
    details->set_file_kind(FILE_KIND_UNSUPPORTED);
  else
    details->set_file_kind(FILE_KIND_FILE);

  details->set_md5(file_resource.md5_checksum());
  details->set_etag(file_resource.etag());
  details->set_creation_time(file_resource.created_date().ToInternalValue());
  details->set_modification_time(
      file_resource.modified_date().ToInternalValue());
  details->set_missing(file_resource.labels().is_trashed());
}

scoped_ptr<FileMetadata> CreateFileMetadataFromFileResource(
    int64 change_id,
    const google_apis::FileResource& resource) {
  scoped_ptr<FileMetadata> file(new FileMetadata);
  file->set_file_id(resource.file_id());

  FileDetails* details = file->mutable_details();
  details->set_change_id(change_id);

  if (resource.labels().is_trashed()) {
    details->set_missing(true);
    return file.Pass();
  }

  PopulateFileDetailsByFileResource(resource, details);
  return file.Pass();
}

scoped_ptr<FileMetadata> CreateFileMetadataFromChangeResource(
    const google_apis::ChangeResource& change) {
  scoped_ptr<FileMetadata> file(new FileMetadata);
  file->set_file_id(change.file_id());

  FileDetails* details = file->mutable_details();
  details->set_change_id(change.change_id());

  if (change.is_deleted()) {
    details->set_missing(true);
    return file.Pass();
  }

  PopulateFileDetailsByFileResource(*change.file(), details);
  return file.Pass();
}

scoped_ptr<FileMetadata> CreateDeletedFileMetadata(
    int64 change_id,
    const std::string& file_id) {
  scoped_ptr<FileMetadata> file(new FileMetadata);
  file->set_file_id(file_id);

  FileDetails* details = file->mutable_details();
  details->set_change_id(change_id);
  details->set_missing(true);
  return file.Pass();
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

scoped_ptr<FileTracker> CloneFileTracker(const FileTracker* obj) {
  if (!obj)
    return scoped_ptr<FileTracker>();
  return scoped_ptr<FileTracker>(new FileTracker(*obj));
}

// Returns true if |db| has no content.
bool IsDatabaseEmpty(LevelDBWrapper* db) {
  DCHECK(db);
  scoped_ptr<LevelDBWrapper::Iterator> itr(db->NewIterator());
  itr->SeekToFirst();
  return !itr->Valid();
}

SyncStatusCode OpenDatabase(const base::FilePath& path,
                            leveldb::Env* env_override,
                            scoped_ptr<LevelDBWrapper>* db_out,
                            bool* created) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db_out);
  DCHECK(created);
  DCHECK(path.IsAbsolute());

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

  db_out->reset(new LevelDBWrapper(make_scoped_ptr(db)));
  *created = IsDatabaseEmpty(db_out->get());
  return status;
}

SyncStatusCode MigrateDatabaseIfNeeded(LevelDBWrapper* db) {
  // See metadata_database_index.cc for the database schema.
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  std::string value;
  leveldb::Status status = db->Get(kDatabaseVersionKey, &value);
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
      drive_backend::MigrateDatabaseFromV0ToV1(db->GetLevelDB());
      // fall-through
    case 1:
      drive_backend::MigrateDatabaseFromV1ToV2(db->GetLevelDB());
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
      // If MetadataDatabaseOnDisk is enabled, migration will be done in
      // MetadataDatabaseOnDisk::Create().
      // TODO(peria): Move the migration code (from v3 to v4) here.
      return SYNC_STATUS_OK;
    case 4:
      if (!CommandLine::ForCurrentProcess()->HasSwitch(
              kEnableMetadataDatabaseOnDisk)) {
        MigrateDatabaseFromV4ToV3(db->GetLevelDB());
      }
      return SYNC_STATUS_OK;
    default:
      return SYNC_DATABASE_ERROR_FAILED;
  }
}

bool HasInvalidTitle(const std::string& title) {
  return title.empty() ||
      title.find('/') != std::string::npos ||
      title.find('\\') != std::string::npos;
}

void MarkTrackerSetDirty(const TrackerIDSet& trackers,
                         MetadataDatabaseIndexInterface* index) {
  for (TrackerIDSet::const_iterator itr = trackers.begin();
       itr != trackers.end(); ++itr) {
    scoped_ptr<FileTracker> tracker(new FileTracker);
    index->GetFileTracker(*itr, tracker.get());
    if (tracker->dirty())
      continue;
    tracker->set_dirty(true);
    index->StoreFileTracker(tracker.Pass());
  }
}

void MarkTrackersDirtyByPath(int64 parent_tracker_id,
                             const std::string& title,
                             MetadataDatabaseIndexInterface* index) {
  if (parent_tracker_id == kInvalidTrackerID || title.empty())
    return;
  MarkTrackerSetDirty(
      index->GetFileTrackerIDsByParentAndTitle(parent_tracker_id, title),
      index);
}

void MarkTrackersDirtyByFileID(const std::string& file_id,
                               MetadataDatabaseIndexInterface* index) {
  MarkTrackerSetDirty(index->GetFileTrackerIDsByFileID(file_id), index);
}

void MarkTrackersDirtyRecursively(int64 root_tracker_id,
                                  MetadataDatabaseIndexInterface* index) {
  std::vector<int64> stack;
  stack.push_back(root_tracker_id);
  while (!stack.empty()) {
    int64 tracker_id = stack.back();
    stack.pop_back();
    AppendContents(index->GetFileTrackerIDsByParent(tracker_id), &stack);

    scoped_ptr<FileTracker> tracker(new FileTracker);
    index->GetFileTracker(tracker_id, tracker.get());
    tracker->set_dirty(true);

    index->StoreFileTracker(tracker.Pass());
  }
}

void RemoveAllDescendantTrackers(int64 root_tracker_id,
                                 MetadataDatabaseIndexInterface* index) {
  std::vector<int64> pending_trackers;
  AppendContents(index->GetFileTrackerIDsByParent(root_tracker_id),
                 &pending_trackers);

  std::vector<int64> to_be_removed;

  // List trackers to remove.
  while (!pending_trackers.empty()) {
    int64 tracker_id = pending_trackers.back();
    pending_trackers.pop_back();
    AppendContents(index->GetFileTrackerIDsByParent(tracker_id),
                   &pending_trackers);
    to_be_removed.push_back(tracker_id);
  }

  // Remove trackers in the reversed order.
  base::hash_set<std::string> affected_file_ids;
  for (std::vector<int64>::reverse_iterator itr = to_be_removed.rbegin();
       itr != to_be_removed.rend(); ++itr) {
    FileTracker tracker;
    index->GetFileTracker(*itr, &tracker);
    affected_file_ids.insert(tracker.file_id());
    index->RemoveFileTracker(*itr);
  }

  for (base::hash_set<std::string>::iterator itr = affected_file_ids.begin();
       itr != affected_file_ids.end(); ++itr) {
    TrackerIDSet trackers = index->GetFileTrackerIDsByFileID(*itr);
    if (trackers.empty()) {
      // Remove metadata that no longer has any tracker.
      index->RemoveFileMetadata(*itr);
    } else {
      MarkTrackerSetDirty(trackers, index);
    }
  }
}

bool FilterFileTrackersByParent(
    const MetadataDatabaseIndexInterface* index,
    const TrackerIDSet& trackers,
    int64 parent_tracker_id,
    FileTracker* tracker_out) {
  FileTracker tracker;
  for (TrackerIDSet::const_iterator itr = trackers.begin();
       itr != trackers.end(); ++itr) {
    if (!index->GetFileTracker(*itr, &tracker)) {
      NOTREACHED();
      continue;
    }

    if (tracker.parent_tracker_id() == parent_tracker_id) {
      if (tracker_out)
        tracker_out->CopyFrom(tracker);
      return true;
    }
  }
  return false;
}

bool FilterFileTrackersByParentAndTitle(
    const MetadataDatabaseIndexInterface* index,
    const TrackerIDSet& trackers,
    int64 parent_tracker_id,
    const std::string& title,
    FileTracker* result) {
  bool found = false;
  for (TrackerIDSet::const_iterator itr = trackers.begin();
       itr != trackers.end(); ++itr) {
    FileTracker tracker;
    if (!index->GetFileTracker(*itr, &tracker)) {
      NOTREACHED();
      continue;
    }

    if (tracker.parent_tracker_id() != parent_tracker_id)
      continue;

    if (tracker.has_synced_details() &&
        tracker.synced_details().title() != title)
      continue;

    // Prioritize trackers that has |synced_details| when |trackers| has
    // multiple candidates.
    if (!found || tracker.has_synced_details()) {
      found = true;
      if (result)
        result->CopyFrom(tracker);
      if (!result || result->has_synced_details())
        return found;
    }
  }

  return found;
}

bool FilterFileTrackersByFileID(
    const MetadataDatabaseIndexInterface* index,
    const TrackerIDSet& trackers,
    const std::string& file_id,
    FileTracker* tracker_out) {
  FileTracker tracker;
  for (TrackerIDSet::const_iterator itr = trackers.begin();
       itr != trackers.end(); ++itr) {
    if (!index->GetFileTracker(*itr, &tracker)) {
      NOTREACHED();
      continue;
    }

    if (tracker.file_id() == file_id) {
      if (tracker_out)
        tracker_out->CopyFrom(tracker);
      return true;
    }
  }
  return false;
}

enum DirtyingOption {
  MARK_NOTHING_DIRTY = 0,
  MARK_ITSELF_DIRTY = 1 << 0,
  MARK_SAME_FILE_ID_TRACKERS_DIRTY = 1 << 1,
  MARK_SAME_PATH_TRACKERS_DIRTY = 1 << 2,
};

void ActivateFileTracker(int64 tracker_id,
                         int dirtying_options,
                         MetadataDatabaseIndexInterface* index) {
  DCHECK(dirtying_options == MARK_NOTHING_DIRTY ||
         dirtying_options == MARK_ITSELF_DIRTY);

  scoped_ptr<FileTracker> tracker(new FileTracker);
  index->GetFileTracker(tracker_id, tracker.get());
  tracker->set_active(true);
  if (dirtying_options & MARK_ITSELF_DIRTY) {
    tracker->set_dirty(true);
    tracker->set_needs_folder_listing(
        tracker->has_synced_details() &&
        tracker->synced_details().file_kind() == FILE_KIND_FOLDER);
  } else {
    tracker->set_dirty(false);
    tracker->set_needs_folder_listing(false);
  }

  index->StoreFileTracker(tracker.Pass());
}

void DeactivateFileTracker(int64 tracker_id,
                           int dirtying_options,
                           MetadataDatabaseIndexInterface* index) {
  RemoveAllDescendantTrackers(tracker_id, index);

  scoped_ptr<FileTracker> tracker(new FileTracker);
  index->GetFileTracker(tracker_id, tracker.get());

  if (dirtying_options & MARK_SAME_FILE_ID_TRACKERS_DIRTY)
    MarkTrackersDirtyByFileID(tracker->file_id(), index);
  if (dirtying_options & MARK_SAME_PATH_TRACKERS_DIRTY) {
    MarkTrackersDirtyByPath(tracker->parent_tracker_id(),
                            GetTrackerTitle(*tracker), index);
  }

  tracker->set_dirty(dirtying_options & MARK_ITSELF_DIRTY);
  tracker->set_active(false);
  index->StoreFileTracker(tracker.Pass());
}

void RemoveFileTracker(int64 tracker_id,
                       int dirtying_options,
                       MetadataDatabaseIndexInterface* index) {
  DCHECK(!(dirtying_options & MARK_ITSELF_DIRTY));

  FileTracker tracker;
  if (!index->GetFileTracker(tracker_id, &tracker))
    return;

  std::string file_id = tracker.file_id();
  int64 parent_tracker_id = tracker.parent_tracker_id();
  std::string title = GetTrackerTitle(tracker);

  RemoveAllDescendantTrackers(tracker_id, index);
  index->RemoveFileTracker(tracker_id);

  if (dirtying_options & MARK_SAME_FILE_ID_TRACKERS_DIRTY)
    MarkTrackersDirtyByFileID(file_id, index);
  if (dirtying_options & MARK_SAME_PATH_TRACKERS_DIRTY)
    MarkTrackersDirtyByPath(parent_tracker_id, title, index);

  if (index->GetFileTrackerIDsByFileID(file_id).empty()) {
    index->RemoveFileMetadata(file_id);
  }
}

}  // namespace

struct MetadataDatabase::CreateParam {
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner;
  base::FilePath database_path;
  leveldb::Env* env_override;

  CreateParam(base::SequencedTaskRunner* worker_task_runner,
              const base::FilePath& database_path,
              leveldb::Env* env_override)
      : worker_task_runner(worker_task_runner),
        database_path(database_path),
        env_override(env_override) {
  }
};

// static
void MetadataDatabase::Create(base::SequencedTaskRunner* worker_task_runner,
                              const base::FilePath& database_path,
                              leveldb::Env* env_override,
                              const CreateCallback& callback) {
  worker_task_runner->PostTask(FROM_HERE, base::Bind(
      &MetadataDatabase::CreateOnWorkerTaskRunner,
      base::Passed(make_scoped_ptr(new CreateParam(
          worker_task_runner,
          database_path,
          env_override))),
      callback));
}

// static
SyncStatusCode MetadataDatabase::CreateForTesting(
    scoped_ptr<LevelDBWrapper> db,
    scoped_ptr<MetadataDatabase>* metadata_database_out) {
  scoped_ptr<MetadataDatabase> metadata_database(
      new MetadataDatabase(base::ThreadTaskRunnerHandle::Get(),
                           base::FilePath(), NULL));
  metadata_database->db_ = db.Pass();
  SyncStatusCode status = metadata_database->Initialize();
  if (status == SYNC_STATUS_OK)
    *metadata_database_out = metadata_database.Pass();
  return status;
}

MetadataDatabase::~MetadataDatabase() {
  worker_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

// static
void MetadataDatabase::ClearDatabase(
    scoped_ptr<MetadataDatabase> metadata_database) {
  DCHECK(metadata_database);
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner =
      metadata_database->worker_task_runner_;
  base::FilePath database_path = metadata_database->database_path_;
  DCHECK(!database_path.empty());
  metadata_database.reset();

  worker_task_runner->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(base::DeleteFile),
                 database_path, true /* recursive */));
}

int64 MetadataDatabase::GetLargestFetchedChangeID() const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->GetLargestChangeID();
}

int64 MetadataDatabase::GetSyncRootTrackerID() const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->GetSyncRootTrackerID();
}

int64 MetadataDatabase::GetLargestKnownChangeID() const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_LE(GetLargestFetchedChangeID(), largest_known_change_id_);
  return largest_known_change_id_;
}

void MetadataDatabase::UpdateLargestKnownChangeID(int64 change_id) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  if (largest_known_change_id_ < change_id)
    largest_known_change_id_ = change_id;
}

bool MetadataDatabase::HasSyncRoot() const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->GetSyncRootTrackerID() != kInvalidTrackerID;
}

void MetadataDatabase::PopulateInitialData(
    int64 largest_change_id,
    const google_apis::FileResource& sync_root_folder,
    const ScopedVector<google_apis::FileResource>& app_root_folders,
    const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  index_->SetLargestChangeID(largest_change_id);
  UpdateLargestKnownChangeID(largest_change_id);

  AttachSyncRoot(sync_root_folder);
  for (size_t i = 0; i < app_root_folders.size(); ++i)
    AttachInitialAppRoot(*app_root_folders[i]);

  WriteToDatabase(callback);
}

bool MetadataDatabase::IsAppEnabled(const std::string& app_id) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  int64 tracker_id = index_->GetAppRootTracker(app_id);
  if (tracker_id == kInvalidTrackerID)
    return false;

  FileTracker tracker;
  if (!index_->GetFileTracker(tracker_id, &tracker))
    return false;
  return tracker.tracker_kind() == TRACKER_KIND_APP_ROOT;
}

void MetadataDatabase::RegisterApp(const std::string& app_id,
                                   const std::string& folder_id,
                                   const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  if (index_->GetAppRootTracker(app_id)) {
    // The app-root is already registered.
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  TrackerIDSet trackers = index_->GetFileTrackerIDsByFileID(folder_id);
  if (trackers.empty()) {
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  if (trackers.has_active()) {
    // The folder is tracked by another tracker.
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to register App for %s", app_id.c_str());
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_STATUS_HAS_CONFLICT));
    return;
  }

  int64 sync_root_tracker_id = index_->GetSyncRootTrackerID();
  if (!sync_root_tracker_id) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Sync-root needs to be set up before registering app-root");
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  scoped_ptr<FileTracker> tracker(new FileTracker);
  if (!FilterFileTrackersByParent(index_.get(), trackers,
                                  sync_root_tracker_id, tracker.get())) {
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  tracker->set_app_id(app_id);
  tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);
  tracker->set_active(true);
  tracker->set_needs_folder_listing(true);
  tracker->set_dirty(true);

  index_->StoreFileTracker(tracker.Pass());
  WriteToDatabase(callback);
}

void MetadataDatabase::DisableApp(const std::string& app_id,
                                  const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  int64 tracker_id = index_->GetAppRootTracker(app_id);
  scoped_ptr<FileTracker> tracker(new FileTracker);
  if (!index_->GetFileTracker(tracker_id, tracker.get())) {
    callback.Run(SYNC_DATABASE_ERROR_NOT_FOUND);
    return;
  }

  if (tracker->tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  DCHECK_EQ(TRACKER_KIND_APP_ROOT, tracker->tracker_kind());
  DCHECK(tracker->active());

  // Keep the app-root tracker active (but change the tracker_kind) so that
  // other conflicting trackers won't become active.
  tracker->set_tracker_kind(TRACKER_KIND_DISABLED_APP_ROOT);

  index_->StoreFileTracker(tracker.Pass());
  WriteToDatabase(callback);
}

void MetadataDatabase::EnableApp(const std::string& app_id,
                                 const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  int64 tracker_id = index_->GetAppRootTracker(app_id);
  scoped_ptr<FileTracker> tracker(new FileTracker);
  if (!index_->GetFileTracker(tracker_id, tracker.get())) {
    callback.Run(SYNC_DATABASE_ERROR_NOT_FOUND);
    return;
  }

  if (tracker->tracker_kind() == TRACKER_KIND_APP_ROOT) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  DCHECK_EQ(TRACKER_KIND_DISABLED_APP_ROOT, tracker->tracker_kind());
  DCHECK(tracker->active());

  tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);
  index_->StoreFileTracker(tracker.Pass());

  MarkTrackersDirtyRecursively(tracker_id, index_.get());
  WriteToDatabase(callback);
}

void MetadataDatabase::UnregisterApp(const std::string& app_id,
                                     const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  int64 tracker_id = index_->GetAppRootTracker(app_id);
  scoped_ptr<FileTracker> tracker(new FileTracker);
  if (!index_->GetFileTracker(tracker_id, tracker.get()) ||
      tracker->tracker_kind() == TRACKER_KIND_REGULAR) {
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  RemoveAllDescendantTrackers(tracker_id, index_.get());

  tracker->clear_app_id();
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  tracker->set_active(false);
  tracker->set_dirty(true);

  index_->StoreFileTracker(tracker.Pass());
  WriteToDatabase(callback);
}

bool MetadataDatabase::FindAppRootTracker(const std::string& app_id,
                                          FileTracker* tracker_out) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  int64 app_root_tracker_id = index_->GetAppRootTracker(app_id);
  if (!app_root_tracker_id)
    return false;

  if (tracker_out &&
      !index_->GetFileTracker(app_root_tracker_id, tracker_out)) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool MetadataDatabase::FindFileByFileID(const std::string& file_id,
                                        FileMetadata* metadata_out) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->GetFileMetadata(file_id, metadata_out);
}

bool MetadataDatabase::FindTrackersByFileID(const std::string& file_id,
                                            TrackerIDSet* trackers_out) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  TrackerIDSet trackers = index_->GetFileTrackerIDsByFileID(file_id);
  if (trackers.empty())
    return false;

  if (trackers_out)
    std::swap(trackers, *trackers_out);
  return true;
}

bool MetadataDatabase::FindTrackersByParentAndTitle(
    int64 parent_tracker_id,
    const std::string& title,
    TrackerIDSet* trackers_out) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  TrackerIDSet trackers =
      index_->GetFileTrackerIDsByParentAndTitle(parent_tracker_id, title);
  if (trackers.empty())
    return false;

  if (trackers_out)
    std::swap(trackers, *trackers_out);
  return true;
}

bool MetadataDatabase::FindTrackerByTrackerID(int64 tracker_id,
                                              FileTracker* tracker_out) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->GetFileTracker(tracker_id, tracker_out);
}

bool MetadataDatabase::BuildPathForTracker(int64 tracker_id,
                                           base::FilePath* path) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

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
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

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
    FileTracker* tracker_out,
    base::FilePath* path_out) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(tracker_out);
  DCHECK(path_out);

  if (full_path.IsAbsolute() ||
      !FindAppRootTracker(app_id, tracker_out) ||
      tracker_out->tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT) {
    return false;
  }

  std::vector<base::FilePath::StringType> components;
  full_path.GetComponents(&components);
  path_out->clear();

  for (size_t i = 0; i < components.size(); ++i) {
    const std::string title = base::FilePath(components[i]).AsUTF8Unsafe();
    TrackerIDSet trackers;
    if (!FindTrackersByParentAndTitle(
            tracker_out->tracker_id(), title, &trackers) ||
        !trackers.has_active()) {
      return true;
    }

    FileTracker tracker;
    index_->GetFileTracker(trackers.active_tracker(), &tracker);

    DCHECK(tracker.has_synced_details());
    const FileDetails& details = tracker.synced_details();
    if (details.file_kind() != FILE_KIND_FOLDER && i != components.size() - 1) {
      // This non-last component indicates file. Give up search.
      return true;
    }

    tracker_out->CopyFrom(tracker);
    *path_out = path_out->Append(components[i]);
  }

  return true;
}

void MetadataDatabase::UpdateByChangeList(
    int64 largest_change_id,
    ScopedVector<google_apis::ChangeResource> changes,
    const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_LE(index_->GetLargestChangeID(), largest_change_id);

  for (size_t i = 0; i < changes.size(); ++i) {
    const google_apis::ChangeResource& change = *changes[i];
    if (HasNewerFileMetadata(change.file_id(), change.change_id()))
      continue;

    scoped_ptr<FileMetadata> metadata(
        CreateFileMetadataFromChangeResource(change));
    UpdateByFileMetadata(FROM_HERE, metadata.Pass(),
                         UPDATE_TRACKER_FOR_UNSYNCED_FILE);
  }

  UpdateLargestKnownChangeID(largest_change_id);
  index_->SetLargestChangeID(largest_change_id);
  WriteToDatabase(callback);
}

void MetadataDatabase::UpdateByFileResource(
    const google_apis::FileResource& resource,
    const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  scoped_ptr<FileMetadata> metadata(
      CreateFileMetadataFromFileResource(
          GetLargestKnownChangeID(), resource));
  UpdateByFileMetadata(FROM_HERE, metadata.Pass(),
                       UPDATE_TRACKER_FOR_UNSYNCED_FILE);
  WriteToDatabase(callback);
}

void MetadataDatabase::UpdateByFileResourceList(
    ScopedVector<google_apis::FileResource> resources,
    const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  for (size_t i = 0; i < resources.size(); ++i) {
    scoped_ptr<FileMetadata> metadata(
        CreateFileMetadataFromFileResource(
            GetLargestKnownChangeID(), *resources[i]));
    UpdateByFileMetadata(FROM_HERE, metadata.Pass(),
                         UPDATE_TRACKER_FOR_UNSYNCED_FILE);
  }
  WriteToDatabase(callback);
}

void MetadataDatabase::UpdateByDeletedRemoteFile(
    const std::string& file_id,
    const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  scoped_ptr<FileMetadata> metadata(
      CreateDeletedFileMetadata(GetLargestKnownChangeID(), file_id));
  UpdateByFileMetadata(FROM_HERE, metadata.Pass(),
                       UPDATE_TRACKER_FOR_UNSYNCED_FILE);
  WriteToDatabase(callback);
}

void MetadataDatabase::UpdateByDeletedRemoteFileList(
    const FileIDList& file_ids,
    const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  for (FileIDList::const_iterator itr = file_ids.begin();
       itr != file_ids.end(); ++itr) {
    scoped_ptr<FileMetadata> metadata(
        CreateDeletedFileMetadata(GetLargestKnownChangeID(), *itr));
    UpdateByFileMetadata(FROM_HERE, metadata.Pass(),
                         UPDATE_TRACKER_FOR_UNSYNCED_FILE);
  }
  WriteToDatabase(callback);
}

void MetadataDatabase::ReplaceActiveTrackerWithNewResource(
    int64 parent_tracker_id,
    const google_apis::FileResource& resource,
    const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  DCHECK(!index_->GetFileMetadata(resource.file_id(), NULL));
  DCHECK(index_->GetFileTracker(parent_tracker_id, NULL));

  UpdateByFileMetadata(
      FROM_HERE,
      CreateFileMetadataFromFileResource(GetLargestKnownChangeID(), resource),
      UPDATE_TRACKER_FOR_SYNCED_FILE);

  DCHECK(index_->GetFileMetadata(resource.file_id(), NULL));
  DCHECK(!index_->GetFileTrackerIDsByFileID(resource.file_id()).has_active());

  TrackerIDSet same_path_trackers =
      index_->GetFileTrackerIDsByParentAndTitle(
          parent_tracker_id, resource.title());
  FileTracker to_be_activated;
  if (!FilterFileTrackersByFileID(index_.get(), same_path_trackers,
                                  resource.file_id(), &to_be_activated)) {
    NOTREACHED();
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_STATUS_FAILED));
    return;
  }

  int64 tracker_id = to_be_activated.tracker_id();
  if (same_path_trackers.has_active()) {
    DeactivateFileTracker(same_path_trackers.active_tracker(),
                          MARK_ITSELF_DIRTY |
                          MARK_SAME_FILE_ID_TRACKERS_DIRTY,
                          index_.get());
  }

  ActivateFileTracker(tracker_id, MARK_NOTHING_DIRTY, index_.get());
  WriteToDatabase(callback);
}

void MetadataDatabase::PopulateFolderByChildList(
    const std::string& folder_id,
    const FileIDList& child_file_ids,
    const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  TrackerIDSet trackers = index_->GetFileTrackerIDsByFileID(folder_id);
  if (!trackers.has_active()) {
    // It's OK that there is no folder to populate its children.
    // Inactive folders should ignore their contents updates.
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  scoped_ptr<FileTracker> folder_tracker(new FileTracker);
  if (!index_->GetFileTracker(trackers.active_tracker(),
                              folder_tracker.get())) {
    NOTREACHED();
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_STATUS_FAILED));
    return;
  }

  base::hash_set<std::string> children(child_file_ids.begin(),
                                       child_file_ids.end());

  std::vector<int64> known_children =
      index_->GetFileTrackerIDsByParent(folder_tracker->tracker_id());
  for (size_t i = 0; i < known_children.size(); ++i) {
    FileTracker tracker;
    if (!index_->GetFileTracker(known_children[i], &tracker)) {
      NOTREACHED();
      continue;
    }
    children.erase(tracker.file_id());
  }

  for (base::hash_set<std::string>::const_iterator itr = children.begin();
       itr != children.end(); ++itr)
    CreateTrackerForParentAndFileID(*folder_tracker, *itr);
  folder_tracker->set_needs_folder_listing(false);
  if (folder_tracker->dirty() && !ShouldKeepDirty(*folder_tracker))
    folder_tracker->set_dirty(false);
  index_->StoreFileTracker(folder_tracker.Pass());

  WriteToDatabase(callback);
}

void MetadataDatabase::UpdateTracker(int64 tracker_id,
                                     const FileDetails& updated_details,
                                     const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  FileTracker tracker;
  if (!index_->GetFileTracker(tracker_id, &tracker)) {
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  // Check if the tracker is to be deleted.
  if (updated_details.missing()) {
    FileMetadata metadata;
    if (!index_->GetFileMetadata(tracker.file_id(), &metadata) ||
        metadata.details().missing()) {
      // Both the tracker and metadata have the missing flag, now it's safe to
      // delete the |tracker|.
      RemoveFileTracker(tracker_id,
                        MARK_SAME_FILE_ID_TRACKERS_DIRTY |
                        MARK_SAME_PATH_TRACKERS_DIRTY,
                        index_.get());
      WriteToDatabase(callback);
      return;
    }
  }

  // Sync-root deletion should be handled separately by SyncEngine.
  DCHECK(tracker_id != GetSyncRootTrackerID() ||
         (tracker.has_synced_details() &&
          tracker.synced_details().title() == updated_details.title() &&
          !updated_details.missing()));

  if (tracker_id != GetSyncRootTrackerID()) {
    // Check if the tracker's parent is still in |parent_tracker_ids|.
    // If not, there should exist another tracker for the new parent, so delete
    // old tracker.
    FileTracker parent_tracker;
    index_->GetFileTracker(tracker.parent_tracker_id(), &parent_tracker);

    if (!HasFileAsParent(updated_details, parent_tracker.file_id())) {
      RemoveFileTracker(tracker.tracker_id(),
                        MARK_SAME_PATH_TRACKERS_DIRTY,
                        index_.get());
      WriteToDatabase(callback);
      return;
    }

    if (tracker.has_synced_details()) {
      // Check if the tracker was retitled.  If it was, there should exist
      // another tracker for the new title, so delete the tracker being updated.
      if (tracker.synced_details().title() != updated_details.title()) {
        RemoveFileTracker(tracker.tracker_id(),
                          MARK_SAME_FILE_ID_TRACKERS_DIRTY,
                          index_.get());
        WriteToDatabase(callback);
        return;
      }
    } else {
      // Check if any other tracker exists has the same parent, title and
      // file_id to the updated tracker.  If it exists, delete the tracker being
      // updated.
      if (FilterFileTrackersByFileID(
              index_.get(),
              index_->GetFileTrackerIDsByParentAndTitle(
                  parent_tracker.tracker_id(),
                  updated_details.title()),
              tracker.file_id(),
              NULL)) {
        RemoveFileTracker(tracker.tracker_id(),
                          MARK_NOTHING_DIRTY,
                          index_.get());
        WriteToDatabase(callback);
        return;
      }
    }
  }

  scoped_ptr<FileTracker> updated_tracker = CloneFileTracker(&tracker);
  *updated_tracker->mutable_synced_details() = updated_details;

  bool should_promote = false;

  // Activate the tracker if:
  //   - There is no active tracker that tracks |tracker->file_id()|.
  //   - There is no active tracker that has the same |parent| and |title|.
  if (!tracker.active() && CanActivateTracker(tracker)) {
    updated_tracker->set_active(true);
    updated_tracker->set_dirty(true);
    updated_tracker->set_needs_folder_listing(
        tracker.synced_details().file_kind() == FILE_KIND_FOLDER);
    should_promote = true;
  } else if (tracker.dirty() && !ShouldKeepDirty(tracker)) {
    updated_tracker->set_dirty(false);
  }
  index_->StoreFileTracker(updated_tracker.Pass());
  if (should_promote)
    index_->PromoteDemotedDirtyTracker(tracker_id);

  WriteToDatabase(callback);
}

MetadataDatabase::ActivationStatus MetadataDatabase::TryActivateTracker(
    int64 parent_tracker_id,
    const std::string& file_id,
    const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  FileMetadata metadata;
  if (!index_->GetFileMetadata(file_id, &metadata)) {
    NOTREACHED();
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_STATUS_FAILED));
    return ACTIVATION_PENDING;
  }
  std::string title = metadata.details().title();
  DCHECK(!HasInvalidTitle(title));

  TrackerIDSet same_file_id_trackers =
      index_->GetFileTrackerIDsByFileID(file_id);
  scoped_ptr<FileTracker> tracker_to_be_activated(new FileTracker);
  FilterFileTrackersByParentAndTitle(
      index_.get(), same_file_id_trackers, parent_tracker_id,
      title, tracker_to_be_activated.get());

  // Check if there is another active tracker that tracks |file_id|.
  // This can happen when the tracked file has multiple parents.
  // In this case, report the failure to the caller.
  if (!tracker_to_be_activated->active() && same_file_id_trackers.has_active())
    return ACTIVATION_FAILED_ANOTHER_ACTIVE_TRACKER;

  if (!tracker_to_be_activated->active()) {
    // Check if there exists another active tracker that has the same path to
    // the tracker.  If there is, deactivate it, assuming the caller already
    // overrides local file with newly added file,
    TrackerIDSet same_title_trackers =
        index_->GetFileTrackerIDsByParentAndTitle(parent_tracker_id, title);
    if (same_title_trackers.has_active()) {
      RemoveAllDescendantTrackers(same_title_trackers.active_tracker(),
                                  index_.get());

      scoped_ptr<FileTracker> tracker_to_be_deactivated(new FileTracker);
      if (index_->GetFileTracker(same_title_trackers.active_tracker(),
                                 tracker_to_be_deactivated.get())) {
        const std::string file_id = tracker_to_be_deactivated->file_id();
        tracker_to_be_deactivated->set_active(false);
        index_->StoreFileTracker(tracker_to_be_deactivated.Pass());

        MarkTrackersDirtyByFileID(file_id, index_.get());
      } else {
        NOTREACHED();
      }
    }
  }

  tracker_to_be_activated->set_dirty(false);
  tracker_to_be_activated->set_active(true);
  *tracker_to_be_activated->mutable_synced_details() = metadata.details();
  if (tracker_to_be_activated->synced_details().file_kind() ==
      FILE_KIND_FOLDER) {
    tracker_to_be_activated->set_needs_folder_listing(true);
  }
  tracker_to_be_activated->set_dirty(false);

  index_->StoreFileTracker(tracker_to_be_activated.Pass());

  WriteToDatabase(callback);
  return ACTIVATION_PENDING;
}

void MetadataDatabase::LowerTrackerPriority(int64 tracker_id) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  index_->DemoteDirtyTracker(tracker_id);
  WriteToDatabase(base::Bind(&EmptyStatusCallback));
}

bool MetadataDatabase::PromoteLowerPriorityTrackersToNormal() {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  bool promoted = index_->PromoteDemotedDirtyTrackers();
  WriteToDatabase(base::Bind(&EmptyStatusCallback));
  return promoted;
}

void MetadataDatabase::PromoteDemotedTracker(int64 tracker_id) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  index_->PromoteDemotedDirtyTracker(tracker_id);
  WriteToDatabase(base::Bind(&EmptyStatusCallback));
}

bool MetadataDatabase::GetNormalPriorityDirtyTracker(
    FileTracker* tracker_out) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  int64 dirty_tracker_id = index_->PickDirtyTracker();
  if (!dirty_tracker_id)
    return false;

  if (tracker_out) {
    if (!index_->GetFileTracker(dirty_tracker_id, tracker_out)) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool MetadataDatabase::HasLowPriorityDirtyTracker() const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->HasDemotedDirtyTracker();
}

bool MetadataDatabase::HasDirtyTracker() const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->PickDirtyTracker() != kInvalidTrackerID;
}

size_t MetadataDatabase::CountDirtyTracker() const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->CountDirtyTracker();
}

bool MetadataDatabase::GetMultiParentFileTrackers(std::string* file_id_out,
                                                  TrackerIDSet* trackers_out) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(file_id_out);
  DCHECK(trackers_out);

  std::string file_id = index_->PickMultiTrackerFileID();
  if (file_id.empty())
    return false;

  TrackerIDSet trackers = index_->GetFileTrackerIDsByFileID(file_id);
  if (trackers.size() <= 1) {
    NOTREACHED();
    return false;
  }

  *file_id_out = file_id;
  std::swap(*trackers_out, trackers);
  return true;
}

size_t MetadataDatabase::CountFileMetadata() const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->CountFileMetadata();
}

size_t MetadataDatabase::CountFileTracker() const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->CountFileTracker();
}

bool MetadataDatabase::GetConflictingTrackers(TrackerIDSet* trackers_out) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(trackers_out);

  ParentIDAndTitle parent_and_title = index_->PickMultiBackingFilePath();
  if (parent_and_title.parent_id == kInvalidTrackerID)
    return false;

  TrackerIDSet trackers = index_->GetFileTrackerIDsByParentAndTitle(
      parent_and_title.parent_id, parent_and_title.title);
  if (trackers.size() <= 1) {
    NOTREACHED();
    return false;
  }

  std::swap(*trackers_out, trackers);
  return true;
}

void MetadataDatabase::GetRegisteredAppIDs(std::vector<std::string>* app_ids) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(app_ids);
  *app_ids = index_->GetRegisteredAppIDs();
}

MetadataDatabase::MetadataDatabase(
    base::SequencedTaskRunner* worker_task_runner,
    const base::FilePath& database_path,
    leveldb::Env* env_override)
    : worker_task_runner_(worker_task_runner),
      database_path_(database_path),
      env_override_(env_override),
      largest_known_change_id_(0),
      weak_ptr_factory_(this) {
  DCHECK(worker_task_runner);
}

// static
void MetadataDatabase::CreateOnWorkerTaskRunner(
    scoped_ptr<CreateParam> create_param,
    const CreateCallback& callback) {
  DCHECK(create_param->worker_task_runner->RunsTasksOnCurrentThread());

  scoped_ptr<MetadataDatabase> metadata_database(
      new MetadataDatabase(create_param->worker_task_runner.get(),
                           create_param->database_path,
                           create_param->env_override));
  SyncStatusCode status = metadata_database->Initialize();
  if (status != SYNC_STATUS_OK)
    metadata_database.reset();

  metadata_database->DetachFromSequence();
  create_param->worker_task_runner->PostTask(
      FROM_HERE,
      base::Bind(
          callback, status, base::Passed(&metadata_database)));
}

SyncStatusCode MetadataDatabase::Initialize() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  SyncStatusCode status = SYNC_STATUS_UNKNOWN;
  bool created = false;
  // Open database unless |db_| is overridden for testing.
  if (!db_) {
    status = OpenDatabase(database_path_, env_override_, &db_, &created);
    if (status != SYNC_STATUS_OK)
      return status;
  }

  if (!created) {
    status = MigrateDatabaseIfNeeded(db_.get());
    if (status != SYNC_STATUS_OK)
      return status;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableMetadataDatabaseOnDisk)) {
    index_ = MetadataDatabaseIndexOnDisk::Create(db_.get());
  } else {
    index_ = MetadataDatabaseIndex::Create(db_.get());
  }

  status = LevelDBStatusToSyncStatusCode(db_->Commit());
  if (status != SYNC_STATUS_OK)
    return status;

  UpdateLargestKnownChangeID(index_->GetLargestChangeID());

  return status;
}

void MetadataDatabase::CreateTrackerForParentAndFileID(
    const FileTracker& parent_tracker,
    const std::string& file_id) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  CreateTrackerInternal(parent_tracker, file_id, NULL,
                        UPDATE_TRACKER_FOR_UNSYNCED_FILE);
}

void MetadataDatabase::CreateTrackerForParentAndFileMetadata(
    const FileTracker& parent_tracker,
    const FileMetadata& file_metadata,
    UpdateOption option) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(file_metadata.has_details());
  CreateTrackerInternal(parent_tracker,
                        file_metadata.file_id(),
                        &file_metadata.details(),
                        option);
}

void MetadataDatabase::CreateTrackerInternal(const FileTracker& parent_tracker,
                                             const std::string& file_id,
                                             const FileDetails* details,
                                             UpdateOption option) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  int64 tracker_id = IncrementTrackerID();
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
  index_->StoreFileTracker(tracker.Pass());
}

void MetadataDatabase::MaybeAddTrackersForNewFile(
    const FileMetadata& metadata,
    UpdateOption option) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  std::set<int64> parents_to_exclude;
  TrackerIDSet existing_trackers =
      index_->GetFileTrackerIDsByFileID(metadata.file_id());
  for (TrackerIDSet::const_iterator itr = existing_trackers.begin();
       itr != existing_trackers.end(); ++itr) {
    FileTracker tracker;
    if (!index_->GetFileTracker(*itr, &tracker)) {
      NOTREACHED();
      continue;
    }

    int64 parent_tracker_id = tracker.parent_tracker_id();
    if (!parent_tracker_id)
      continue;

    // Exclude |parent_tracker_id| if it already has a tracker that has
    // unknown title or has the same title with |file|.
    if (!tracker.has_synced_details() ||
        tracker.synced_details().title() == metadata.details().title()) {
      parents_to_exclude.insert(parent_tracker_id);
    }
  }

  for (int i = 0; i < metadata.details().parent_folder_ids_size(); ++i) {
    std::string parent_folder_id = metadata.details().parent_folder_ids(i);
    TrackerIDSet parent_trackers =
        index_->GetFileTrackerIDsByFileID(parent_folder_id);
    for (TrackerIDSet::const_iterator itr = parent_trackers.begin();
         itr != parent_trackers.end(); ++itr) {
      FileTracker parent_tracker;
      index_->GetFileTracker(*itr, &parent_tracker);
      if (!parent_tracker.active())
        continue;

      if (ContainsKey(parents_to_exclude, parent_tracker.tracker_id()))
        continue;

      CreateTrackerForParentAndFileMetadata(
          parent_tracker, metadata, option);
    }
  }
}

int64 MetadataDatabase::IncrementTrackerID() {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  int64 tracker_id = index_->GetNextTrackerID();
  index_->SetNextTrackerID(tracker_id + 1);
  DCHECK_GT(tracker_id, 0);
  return tracker_id;
}

bool MetadataDatabase::CanActivateTracker(const FileTracker& tracker) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(!tracker.active());
  DCHECK_NE(index_->GetSyncRootTrackerID(), tracker.tracker_id());

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
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  if (HasDisabledAppRoot(tracker))
    return false;

  DCHECK(tracker.dirty());
  if (!tracker.has_synced_details())
    return true;

  FileMetadata metadata;
  if (!index_->GetFileMetadata(tracker.file_id(), &metadata))
    return true;
  DCHECK(metadata.has_details());

  const FileDetails& local_details = tracker.synced_details();
  const FileDetails& remote_details = metadata.details();

  if (tracker.active()) {
    if (tracker.needs_folder_listing())
      return true;
    if (local_details.md5() != remote_details.md5())
      return true;
    if (local_details.missing() != remote_details.missing())
      return true;
  }

  if (local_details.title() != remote_details.title())
    return true;

  return false;
}

bool MetadataDatabase::HasDisabledAppRoot(const FileTracker& tracker) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  int64 app_root_tracker_id = index_->GetAppRootTracker(tracker.app_id());
  if (app_root_tracker_id == kInvalidTrackerID)
    return false;

  FileTracker app_root_tracker;
  if (!index_->GetFileTracker(app_root_tracker_id, &app_root_tracker)) {
    NOTREACHED();
    return false;
  }
  return app_root_tracker.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT;
}

bool MetadataDatabase::HasActiveTrackerForFileID(
    const std::string& file_id) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->GetFileTrackerIDsByFileID(file_id).has_active();
}

bool MetadataDatabase::HasActiveTrackerForPath(int64 parent_tracker_id,
                                               const std::string& title) const {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  return index_->GetFileTrackerIDsByParentAndTitle(parent_tracker_id, title)
      .has_active();
}

void MetadataDatabase::RemoveUnneededTrackersForMissingFile(
    const std::string& file_id) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  TrackerIDSet trackers = index_->GetFileTrackerIDsByFileID(file_id);
  for (TrackerIDSet::const_iterator itr = trackers.begin();
       itr != trackers.end(); ++itr) {
    FileTracker tracker;
    if (!index_->GetFileTracker(*itr, &tracker)) {
      NOTREACHED();
      continue;
    }

    if (!tracker.has_synced_details() || tracker.synced_details().missing()) {
      RemoveFileTracker(*itr, MARK_NOTHING_DIRTY, index_.get());
    }
  }
}

void MetadataDatabase::UpdateByFileMetadata(
    const tracked_objects::Location& from_where,
    scoped_ptr<FileMetadata> metadata,
    UpdateOption option) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(metadata);
  DCHECK(metadata->has_details());

  DVLOG(1) << from_where.function_name() << ": "
           << metadata->file_id() << " ("
           << metadata->details().title() << ")"
           << (metadata->details().missing() ? " deleted" : "");

  std::string file_id = metadata->file_id();
  if (metadata->details().missing())
    RemoveUnneededTrackersForMissingFile(file_id);
  else
    MaybeAddTrackersForNewFile(*metadata, option);

  TrackerIDSet trackers = index_->GetFileTrackerIDsByFileID(file_id);
  if (!trackers.empty()) {
    index_->StoreFileMetadata(metadata.Pass());

    if (option != UPDATE_TRACKER_FOR_SYNCED_FILE)
      MarkTrackerSetDirty(trackers, index_.get());
  }
}

void MetadataDatabase::WriteToDatabase(const SyncStatusCallback& callback) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  leveldb::Status status = db_->Commit();
  callback.Run(LevelDBStatusToSyncStatusCode(status));
}

scoped_ptr<base::ListValue> MetadataDatabase::DumpFiles(
    const std::string& app_id) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  scoped_ptr<base::ListValue> files(new base::ListValue);

  FileTracker app_root_tracker;
  if (!FindAppRootTracker(app_id, &app_root_tracker))
    return files.Pass();

  std::vector<int64> stack;
  AppendContents(
      index_->GetFileTrackerIDsByParent(app_root_tracker.tracker_id()), &stack);
  while (!stack.empty()) {
    int64 tracker_id = stack.back();
    stack.pop_back();
    AppendContents(index_->GetFileTrackerIDsByParent(tracker_id), &stack);

    FileTracker tracker;
    if (!index_->GetFileTracker(tracker_id, &tracker)) {
      NOTREACHED();
      continue;
    }
    base::DictionaryValue* file = new base::DictionaryValue;

    base::FilePath path = BuildDisplayPathForTracker(tracker);
    file->SetString("path", path.AsUTF8Unsafe());
    if (tracker.has_synced_details()) {
      file->SetString("title", tracker.synced_details().title());
      file->SetString("type",
                      FileKindToString(tracker.synced_details().file_kind()));
    }

    base::DictionaryValue* details = new base::DictionaryValue;
    details->SetString("file_id", tracker.file_id());
    if (tracker.has_synced_details() &&
        tracker.synced_details().file_kind() == FILE_KIND_FILE)
      details->SetString("md5", tracker.synced_details().md5());
    details->SetString("active", tracker.active() ? "true" : "false");
    details->SetString("dirty", tracker.dirty() ? "true" : "false");

    file->Set("details", details);

    files->Append(file);
  }

  return files.Pass();
}

scoped_ptr<base::ListValue> MetadataDatabase::DumpDatabase() {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  scoped_ptr<base::ListValue> list(new base::ListValue);
  list->Append(DumpTrackers().release());
  list->Append(DumpMetadata().release());
  return list.Pass();
}

bool MetadataDatabase::HasNewerFileMetadata(const std::string& file_id,
                                            int64 change_id) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  FileMetadata metadata;
  if (!index_->GetFileMetadata(file_id, &metadata))
    return false;
  DCHECK(metadata.has_details());
  return metadata.details().change_id() >= change_id;
}

scoped_ptr<base::ListValue> MetadataDatabase::DumpTrackers() {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

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
  std::vector<int64> tracker_ids(index_->GetAllTrackerIDs());
  for (std::vector<int64>::const_iterator itr = tracker_ids.begin();
       itr != tracker_ids.end(); ++itr) {
    const int64 tracker_id = *itr;
    FileTracker tracker;
    if (!index_->GetFileTracker(tracker_id, &tracker)) {
      NOTREACHED();
      continue;
    }

    base::DictionaryValue* dict = new base::DictionaryValue;
    base::FilePath path = BuildDisplayPathForTracker(tracker);
    dict->SetString("tracker_id", base::Int64ToString(tracker_id));
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
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

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
  std::vector<std::string> metadata_ids(index_->GetAllMetadataIDs());
  for (std::vector<std::string>::const_iterator itr = metadata_ids.begin();
       itr != metadata_ids.end(); ++itr) {
    const std::string& file_id = *itr;
    FileMetadata file;
    if (!index_->GetFileMetadata(file_id, &file)) {
      NOTREACHED();
      continue;
    }

    base::DictionaryValue* dict = new base::DictionaryValue;
    dict->SetString("file_id", file_id);
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

void MetadataDatabase::AttachSyncRoot(
    const google_apis::FileResource& sync_root_folder) {
  DCHECK(worker_sequence_checker_.CalledOnValidSequencedThread());

  scoped_ptr<FileMetadata> sync_root_metadata =
      CreateFileMetadataFromFileResource(
          GetLargestKnownChangeID(), sync_root_folder);
  scoped_ptr<FileTracker> sync_root_tracker =
      CreateSyncRootTracker(IncrementTrackerID(), *sync_root_metadata);

  index_->SetSyncRootTrackerID(sync_root_tracker->tracker_id());
  index_->StoreFileMetadata(sync_root_metadata.Pass());
  index_->StoreFileTracker(sync_root_tracker.Pass());
}

void MetadataDatabase::AttachInitialAppRoot(
    const google_apis::FileResource& app_root_folder) {
  scoped_ptr<FileMetadata> app_root_metadata =
      CreateFileMetadataFromFileResource(
          GetLargestKnownChangeID(), app_root_folder);
  scoped_ptr<FileTracker> app_root_tracker =
      CreateInitialAppRootTracker(IncrementTrackerID(),
                                  GetSyncRootTrackerID(),
                                  *app_root_metadata);

  index_->StoreFileMetadata(app_root_metadata.Pass());
  index_->StoreFileTracker(app_root_tracker.Pass());
}

void MetadataDatabase::DetachFromSequence() {
  worker_sequence_checker_.DetachFromSequence();
}

}  // namespace drive_backend
}  // namespace sync_file_system
