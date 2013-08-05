// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

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
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

typedef MetadataDatabase::FileByAppID FileByAppID;
typedef MetadataDatabase::FileByFileID FileByFileID;
typedef MetadataDatabase::FileByParentAndTitle FileByParentAndTitle;
typedef MetadataDatabase::FileSet FileSet;
typedef MetadataDatabase::FilesByParent FilesByParent;

const char kDatabaseVersionKey[] = "VERSION";
const int64 kCurrentDatabaseVersion = 3;
const char kServiceMetadataKey[] = "SERVICE";
const char kFileMetadataKeyPrefix[] = "FILE: ";

struct DatabaseContents {
  scoped_ptr<ServiceMetadata> service_metadata;
  ScopedVector<DriveFileMetadata> file_metadata;
};

namespace {

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

void PopulateFileDetailsFromFileResource(
    int64 change_id,
    const google_apis::FileResource& file_resource,
    DriveFileMetadata::Details* details) {
  details->set_change_id(change_id);
  for (ScopedVector<google_apis::ParentReference>::const_iterator itr =
           file_resource.parents().begin();
       itr != file_resource.parents().end();
       ++itr) {
    details->add_parent_folder_id((*itr)->file_id());
  }
  details->set_title(file_resource.title());

  google_apis::DriveEntryKind kind = file_resource.GetKind();
  if (kind == google_apis::ENTRY_KIND_FILE)
    details->set_kind(KIND_FILE);
  else if (kind == google_apis::ENTRY_KIND_FOLDER)
    details->set_kind(KIND_FOLDER);
  else
    details->set_kind(KIND_UNSUPPORTED);

  details->set_md5(file_resource.md5_checksum());
  details->set_etag(file_resource.etag());
  details->set_creation_time(file_resource.created_date().ToInternalValue());
  details->set_modification_time(
      file_resource.modified_date().ToInternalValue());
  details->set_deleted(false);
}

void AdaptLevelDBStatusToSyncStatusCode(const SyncStatusCallback& callback,
                                        const leveldb::Status& status) {
  callback.Run(LevelDBStatusToSyncStatusCode(status));
}

void PutFileToBatch(const DriveFileMetadata& file, leveldb::WriteBatch* batch) {
  std::string value;
  bool success = file.SerializeToString(&value);
  DCHECK(success);
  batch->Put(kFileMetadataKeyPrefix + file.file_id(), value);
}

void PushChildrenToStack(const FilesByParent& files_by_parent,
                         const std::string& folder_id,
                         std::stack<std::string>* stack) {
  FilesByParent::const_iterator found = files_by_parent.find(folder_id);
  if (found == files_by_parent.end())
    return;
  const FileSet& children = found->second;
  for (FileSet::const_iterator itr = children.begin();
       itr != children.end(); ++itr)
    stack->push((*itr)->file_id());
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

      scoped_ptr<DriveFileMetadata> metadata(new DriveFileMetadata);
      if (!metadata->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a Metadata");
        continue;
      }

      contents->file_metadata.push_back(metadata.release());
      continue;
    }
  }

  return SYNC_STATUS_OK;
}

SyncStatusCode InitializeServiceMetadata(DatabaseContents* contents,
                                         leveldb::WriteBatch* batch) {

  if (!contents->service_metadata) {
    contents->service_metadata.reset(new ServiceMetadata);

    std::string value;
    contents->service_metadata->SerializeToString(&value);
    batch->Put(kServiceMetadataKey, value);
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode RemoveUnreachableFiles(DatabaseContents* contents,
                                      leveldb::WriteBatch* batch) {
  FileByFileID unvisited_files;
  FilesByParent files_by_parent;

  for (ScopedVector<DriveFileMetadata>::iterator itr =
           contents->file_metadata.begin();
       itr != contents->file_metadata.end();
       ++itr) {
    DriveFileMetadata* metadata = *itr;
    DCHECK(!ContainsKey(unvisited_files, metadata->file_id()));
    unvisited_files[metadata->file_id()] = metadata;
    files_by_parent[metadata->parent_folder_id()].insert(metadata);
  }

  // Traverse synced metadata tree. Take only active items and their children.
  // Drop unreachable items.
  ScopedVector<DriveFileMetadata> reachable_files;
  std::stack<std::string> pending;
  if (!contents->service_metadata->sync_root_folder_id().empty())
    pending.push(contents->service_metadata->sync_root_folder_id());

  while (!pending.empty()) {
    std::string file_id = pending.top();
    pending.pop();

    {
      FileByFileID::iterator found = unvisited_files.find(file_id);
      if (found == unvisited_files.end())
        continue;

      DriveFileMetadata* metadata = found->second;
      unvisited_files.erase(found);
      reachable_files.push_back(metadata);

      if (!metadata->active() && !metadata->is_app_root())
        continue;
    }

    FilesByParent::iterator found = files_by_parent.find(file_id);
    if (found == files_by_parent.end())
      continue;

    for (FileSet::const_iterator itr = found->second.begin();
         itr != found->second.end();
         ++itr)
      pending.push((*itr)->file_id());
  }

  for (FileByFileID::iterator itr = unvisited_files.begin();
       itr != unvisited_files.end();
       ++itr) {
    DriveFileMetadata* metadata = itr->second;
    batch->Delete(metadata->file_id());
    delete metadata;
  }
  unvisited_files.clear();

  // |reachable_files| contains all files/folders reachable from sync-root
  // folder via active folders.
  contents->file_metadata.weak_clear();
  contents->file_metadata.swap(reachable_files);

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

void RunSoon(const tracked_objects::Location& from_here,
             const base::Closure& closure) {
  base::MessageLoopProxy::current()->PostTask(from_here, closure);
}

}  // namespace

bool MetadataDatabase::FileIDComparator::operator()(
    DriveFileMetadata* left,
    DriveFileMetadata* right) const {
  return left->file_id() < right->file_id();
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
      file_by_file_id_.begin(), file_by_file_id_.end());
}

int64 MetadataDatabase::GetLargestChangeID() const {
  return service_metadata_->largest_change_id();
}

void MetadataDatabase::RegisterApp(const std::string& app_id,
                                   const std::string& folder_id,
                                   const SyncStatusCallback& callback) {
  if (FindAppRootFolder(app_id, NULL)) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  DriveFileMetadata folder;
  if (!FindFileByFileID(folder_id, &folder)) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  DCHECK(!folder.active());
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  RegisterFolderAsAppRoot(app_id, folder.file_id(), batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::DisableApp(const std::string& app_id,
                                  const SyncStatusCallback& callback) {
  DriveFileMetadata folder;
  if (!FindAppRootFolder(app_id, &folder)) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  if (!folder.active()) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  MakeFileInactive(folder.file_id(), batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::EnableApp(const std::string& app_id,
                                 const SyncStatusCallback& callback) {
  DriveFileMetadata folder;
  if (!FindAppRootFolder(app_id, &folder)) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  if (folder.active()) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  MakeFileActive(folder.file_id(), batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::UnregisterApp(const std::string& app_id,
                                     const SyncStatusCallback& callback) {
  DriveFileMetadata folder;
  if (!FindAppRootFolder(app_id, &folder)) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  UnregisterFolderAsAppRoot(app_id, batch.get());
  WriteToDatabase(batch.Pass(), callback);
}

bool MetadataDatabase::FindAppRootFolder(const std::string& app_id,
                                         DriveFileMetadata* folder) const {
  return FindItem(app_root_by_app_id_, app_id, folder);
}

bool MetadataDatabase::FindFileByFileID(const std::string& file_id,
                                        DriveFileMetadata* metadata) const {
  return FindItem(file_by_file_id_, file_id, metadata);
}

size_t MetadataDatabase::FindFilesByParentAndTitle(
    const std::string& file_id,
    const std::string& title,
    ScopedVector<DriveFileMetadata>* files) const {
  NOTIMPLEMENTED();
  return 0;
}

bool MetadataDatabase::FindActiveFileByParentAndTitle(
    const std::string& folder_id,
    const std::string& title,
    DriveFileMetadata* file) const {
  return FindItem(active_file_by_parent_and_title_,
                  std::make_pair(folder_id, title),
                  file);
}

bool MetadataDatabase::FindActiveFileByPath(const std::string& app_id,
                                            const base::FilePath& path,
                                            DriveFileMetadata* file) const {
  DriveFileMetadata current;
  if (!FindAppRootFolder(app_id, &current))
    return false;

  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);

  std::string parent_folder_id = current.file_id();
  for (std::vector<base::FilePath::StringType>::iterator itr =
           components.begin();
       itr != components.end();
       ++itr) {
    std::string current_folder_id = current.file_id();
    if (!FindActiveFileByParentAndTitle(
            current_folder_id, base::FilePath(*itr).AsUTF8Unsafe(), &current))
      return false;
  }
  if (file)
    *file = current;
  return true;
}

bool MetadataDatabase::BuildPathForFile(const std::string& file_id,
                                        base::FilePath* path) const {
  DriveFileMetadata current;
  if (!FindFileByFileID(file_id, &current) || !current.active())
    return false;

  std::vector<base::FilePath> components;
  while (!current.is_app_root()) {
    components.push_back(base::FilePath::FromUTF8Unsafe(
        current.synced_details().title()));
    if (!FindFileByFileID(current.parent_folder_id(), &current) ||
        !current.active())
      return false;
  }

  if (path)
    *path = ReverseConcatPathComponents(components);

  return true;
}

void MetadataDatabase::UpdateByChangeList(
    ScopedVector<google_apis::ChangeResource> changes,
    const SyncStatusCallback& callback) {
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);

  for (ScopedVector<google_apis::ChangeResource>::iterator itr =
           changes.begin();
       itr != changes.end();
       ++itr) {
    const google_apis::ChangeResource& change = **itr;
    DriveFileMetadata file;

    // If the remote file already has an entry in the database, update its
    // |remote_detais| and mark it dirty.
    if (FindFileByFileID(change.file_id(), NULL)) {
      const google_apis::FileResource* file_resource = NULL;
      if (!change.is_deleted())
        file_resource = change.file();
      UpdateRemoteDetails(change.change_id(), change.file_id(), file_resource,
                          batch.get());
      continue;
    }

    // Deletion of an unknown file can be safely ignorable.
    if (change.is_deleted())
      continue;

    // Find first active parent.
    std::string parent_folder_id;
    DriveFileMetadata parent;
    for (ScopedVector<google_apis::ParentReference>::const_iterator itr =
             change.file()->parents().begin();
         itr != change.file()->parents().end();
         ++itr) {
      if (FindFileByFileID((*itr)->file_id(), &parent) &&
          (parent.active() || parent.is_app_root())) {
        parent_folder_id = parent.file_id();
        break;
      }
    }

    // If the remote file doesn't have active parent, ignore the file.
    if (parent_folder_id.empty())
      continue;

    RegisterNewFile(change.change_id(), parent, *change.file(),
                    batch.get());
  }

  WriteToDatabase(batch.Pass(), callback);
}

void MetadataDatabase::PopulateFolder(
    const std::string& folder_id,
    ScopedVector<google_apis::ResourceEntry> children,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
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

  status = RemoveUnreachableFiles(&contents, &batch);
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
  for (ScopedVector<DriveFileMetadata>::iterator itr =
           contents->file_metadata.begin();
       itr != contents->file_metadata.end();
       ++itr) {
    DriveFileMetadata* file = *itr;
    file_by_file_id_[file->file_id()] = file;

    if (file->is_app_root())
      app_root_by_app_id_[file->app_id()] = file;

    if (file->active() && file->has_synced_details()) {
      FileByParentAndTitle::key_type key(
          file->parent_folder_id(), file->synced_details().title());
      active_file_by_parent_and_title_[key] = file;
    }

    if (!file->parent_folder_id().empty())
      files_by_parent_[file->parent_folder_id()].insert(file);

    if (file->dirty())
      dirty_files_.insert(file);
  }

  contents->file_metadata.weak_clear();
}

void MetadataDatabase::RegisterFolderAsAppRoot(
    const std::string& app_id,
    const std::string& folder_id,
    leveldb::WriteBatch* batch) {
  DriveFileMetadata* folder = file_by_file_id_[folder_id];
  if (!folder || folder->active() || folder->is_app_root() ||
      !folder->app_id().empty()) {
    NOTREACHED();
    return;
  }

  folder->set_is_app_root(true);
  folder->set_app_id(app_id);
  folder->set_active(true);
  folder->set_dirty(true);
  folder->set_needs_folder_listing(true);
  PutFileToBatch(*folder, batch);

  app_root_by_app_id_[app_id] = folder;
  if (folder->has_synced_details()) {
    FileByParentAndTitle::key_type key(
        folder->parent_folder_id(), folder->synced_details().title());
    DCHECK(!ContainsKey(active_file_by_parent_and_title_, key));
    active_file_by_parent_and_title_[key] = folder;
  }
  dirty_files_.insert(folder);
}

void MetadataDatabase::UnregisterFolderAsAppRoot(
    const std::string& app_id,
    leveldb::WriteBatch* batch) {
  DriveFileMetadata* folder = app_root_by_app_id_[app_id];
  if (!folder || !folder->active() ||
      folder->app_id() != app_id || !folder->is_app_root()) {
    NOTREACHED();
    return;
  }

  folder->set_active(false);
  folder->set_is_app_root(false);
  folder->set_app_id(std::string());
  PutFileToBatch(*folder, batch);

  // Remove child entries recursively.
  std::stack<std::string> pending_files;
  PushChildrenToStack(files_by_parent_, folder->file_id(), &pending_files);
  while (!pending_files.empty()) {
    std::string file_id = pending_files.top();
    pending_files.pop();
    PushChildrenToStack(files_by_parent_, file_id, &pending_files);
    RemoveFile(file_id, batch);
  }

  app_root_by_app_id_.erase(app_id);
  if (folder->has_synced_details()) {
    FileByParentAndTitle::key_type key(
        folder->parent_folder_id(), folder->synced_details().title());
    active_file_by_parent_and_title_.erase(key);
  }
}

void MetadataDatabase::MakeFileActive(const std::string& file_id,
                                      leveldb::WriteBatch* batch) {
  DriveFileMetadata* file = file_by_file_id_[file_id];
  if (!file || file->active()) {
    NOTREACHED();
    return;
  }

  file->set_active(true);
  if (file->has_synced_details() &&
      file->synced_details().kind() == KIND_FOLDER)
    file->set_needs_folder_listing(true);
  PutFileToBatch(*file, batch);

  if (file->has_synced_details()) {
    FileByParentAndTitle::key_type key(
        file->parent_folder_id(), file->synced_details().title());
    DCHECK(!ContainsKey(active_file_by_parent_and_title_, key));
    active_file_by_parent_and_title_[key] = file;
  }
}

void MetadataDatabase::MakeFileInactive(const std::string& file_id,
                                        leveldb::WriteBatch* batch) {
  DriveFileMetadata* file = file_by_file_id_[file_id];
  if (!file || !file->active()) {
    NOTREACHED();
    return;
  }

  file->set_active(false);
  PutFileToBatch(*file, batch);

  if (file->has_synced_details()) {
    FileByParentAndTitle::key_type key(
        file->parent_folder_id(), file->synced_details().title());
    DCHECK(ContainsKey(active_file_by_parent_and_title_, key));
    active_file_by_parent_and_title_.erase(key);
  }
}

void MetadataDatabase::RemoveFile(const std::string& file_id,
                                  leveldb::WriteBatch* batch) {
  scoped_ptr<DriveFileMetadata> file(file_by_file_id_[file_id]);
  file_by_file_id_.erase(file_id);

  batch->Delete(file->file_id());

  files_by_parent_[file->parent_folder_id()].erase(file.get());
  if (file->is_app_root())
    app_root_by_app_id_.erase(file->app_id());
  if (file->active() && file->has_synced_details()) {
    FileByParentAndTitle::key_type key(
        file->parent_folder_id(), file->synced_details().title());
    active_file_by_parent_and_title_.erase(key);
  }
  dirty_files_.erase(file.get());
}

void MetadataDatabase::UpdateRemoteDetails(
    int64 change_id,
    const std::string& file_id,
    const google_apis::FileResource* file_resource,
    leveldb::WriteBatch* batch) {
  DriveFileMetadata* file = file_by_file_id_[file_id];

  file->clear_remote_details();
  DriveFileMetadata::Details* details = file->mutable_remote_details();
  if (file_resource) {
    PopulateFileDetailsFromFileResource(change_id, *file_resource, details);
  } else {
    details->set_deleted(true);
    details->set_change_id(change_id);
  }

  file->set_dirty(true);
  PutFileToBatch(*file, batch);

  dirty_files_.insert(file);
}

void MetadataDatabase::RegisterNewFile(
    int64 change_id,
    const DriveFileMetadata& parent_folder,
    const google_apis::FileResource& new_file_resource,
    leveldb::WriteBatch* batch) {
  scoped_ptr<DriveFileMetadata> file(new DriveFileMetadata);
  std::string file_id = new_file_resource.file_id();
  file->set_file_id(file_id);
  file->set_parent_folder_id(parent_folder.file_id());
  file->set_app_id(parent_folder.app_id());
  file->set_is_app_root(false);

  PopulateFileDetailsFromFileResource(
      change_id, new_file_resource, file->mutable_remote_details());

  file->set_dirty(true);
  file->set_active(false);
  file->set_needs_folder_listing(
      new_file_resource.GetKind() == google_apis::ENTRY_KIND_FOLDER);

  PutFileToBatch(*file, batch);

  files_by_parent_[parent_folder.file_id()].insert(file.get());
  dirty_files_.insert(file.get());

  file_by_file_id_[file_id] = file.release();
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

}  // namespace drive_backend
}  // namespace sync_file_system
