// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"

#include <leveldb/db.h>
#include <utility>

#include "base/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/string_number_conversions.h"
#include "base/tracked_objects.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_files.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/time_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace {

// m: prefix for filesystem metadata db keys, version and largest_changestamp.
// r: prefix for resource id db keys.
const char kDBKeyLargestChangestamp[] = "m:largest_changestamp";
const char kDBKeyVersion[] = "m:version";
const char kDBKeyResourceIdPrefix[] = "r:";

// Posts |error| to |callback| asynchronously. |callback| must not be null.
void PostFileMoveCallbackError(const FileMoveCallback& callback,
                               DriveFileError error) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, FilePath()));
}

// Posts |error| to |callback| asynchronously. |callback| must not be null.
void PostGetEntryInfoWithFilePathCallbackError(
    const GetEntryInfoWithFilePathCallback& callback,
    DriveFileError error) {
  scoped_ptr<DriveEntryProto> proto;
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, FilePath(), base::Passed(&proto)));
}

}  // namespace

EntryInfoResult::EntryInfoResult() : error(DRIVE_FILE_ERROR_FAILED) {
}

EntryInfoResult::~EntryInfoResult() {
}

EntryInfoPairResult::EntryInfoPairResult() {
}

EntryInfoPairResult::~EntryInfoPairResult() {
}

// ResourceMetadataDB implementation.

// Params for ResourceMetadataDB::Create.
struct CreateDBParams {
  CreateDBParams(const FilePath& db_path,
                 base::SequencedTaskRunner* blocking_task_runner)
                 : db_path(db_path),
                   blocking_task_runner(blocking_task_runner) {
  }

  FilePath db_path;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner;
  scoped_ptr<ResourceMetadataDB> db;
  DriveResourceMetadata::SerializedMap serialized_resources;
};

// Wrapper for level db. All methods must be called on blocking thread.
class ResourceMetadataDB {
 public:
  ResourceMetadataDB(const FilePath& db_path,
                          base::SequencedTaskRunner* blocking_task_runner);

  // Initializes the database.
  void Init();

  // Reads the database into |serialized_resources|.
  void Read(DriveResourceMetadata::SerializedMap* serialized_resources);

  // Saves |serialized_resources| to the database.
  void Save(const DriveResourceMetadata::SerializedMap& serialized_resources);

 private:
  // Clears the database.
  void Clear();

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<leveldb::DB> level_db_;
  FilePath db_path_;
};

ResourceMetadataDB::ResourceMetadataDB(const FilePath& db_path,
    base::SequencedTaskRunner* blocking_task_runner)
  : blocking_task_runner_(blocking_task_runner),
    db_path_(db_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
}

// Creates, initializes and reads from the database.
// This must be defined after ResourceMetadataDB and CreateDBParams.
static void CreateResourceMetadataDBOnBlockingPool(
    CreateDBParams* params) {
  DCHECK(params->blocking_task_runner->RunsTasksOnCurrentThread());
  DCHECK(!params->db_path.empty());

  params->db.reset(new ResourceMetadataDB(params->db_path,
                                               params->blocking_task_runner));
  params->db->Init();
  params->db->Read(&params->serialized_resources);
}

void ResourceMetadataDB::Init() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!db_path_.empty());

  DVLOG(1) << "Init " << db_path_.value();

  leveldb::DB* level_db = NULL;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status db_status = leveldb::DB::Open(options, db_path_.value(),
                                                &level_db);
  DCHECK(level_db);
  DCHECK(db_status.ok());
  level_db_.reset(level_db);
}

void ResourceMetadataDB::Read(
  DriveResourceMetadata::SerializedMap* serialized_resources) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(serialized_resources);
  DVLOG(1) << "Read " << db_path_.value();

  scoped_ptr<leveldb::Iterator> iter(level_db_->NewIterator(
        leveldb::ReadOptions()));
  for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    DVLOG(1) << "Read, resource " << iter->key().ToString();
    serialized_resources->insert(std::make_pair(iter->key().ToString(),
                                                iter->value().ToString()));
  }
}

void ResourceMetadataDB::Save(
    const DriveResourceMetadata::SerializedMap& serialized_resources) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  Clear();
  for (DriveResourceMetadata::SerializedMap::const_iterator iter =
      serialized_resources.begin();
      iter != serialized_resources.end(); ++iter) {
    DVLOG(1) << "Saving resource " << iter->first << " to db";
    leveldb::Status status = level_db_->Put(leveldb::WriteOptions(),
                                            leveldb::Slice(iter->first),
                                            leveldb::Slice(iter->second));
    if (!status.ok()) {
      LOG(ERROR) << "leveldb Put failed of " << iter->first
                 << ", with " << status.ToString();
      NOTREACHED();
    }
  }
}

void ResourceMetadataDB::Clear() {
  level_db_.reset();
  leveldb::DestroyDB(db_path_.value(), leveldb::Options());
  Init();
}

// DriveResourceMetadata class implementation.

DriveResourceMetadata::DriveResourceMetadata()
    : blocking_task_runner_(NULL),
      serialized_size_(0),
      largest_changestamp_(0),
      loaded_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  root_ = CreateDriveDirectory().Pass();
  root_->set_resource_id(kWAPIRootDirectoryResourceId);
  root_->set_title(kDriveRootDirectory);
  root_->SetBaseNameFromTitle();

  AddEntryToResourceMap(root_.get());
}

DriveResourceMetadata::~DriveResourceMetadata() {
  ClearRoot();

  // Ensure db is closed on the blocking pool.
  if (blocking_task_runner_ && resource_metadata_db_.get())
    blocking_task_runner_->DeleteSoon(FROM_HERE,
                                      resource_metadata_db_.release());
}

scoped_ptr<DriveFile> DriveResourceMetadata::CreateDriveFile() {
  return scoped_ptr<DriveFile>(new DriveFile(this));
}

scoped_ptr<DriveDirectory> DriveResourceMetadata::CreateDriveDirectory() {
  return scoped_ptr<DriveDirectory>(new DriveDirectory(this));
}

void DriveResourceMetadata::ClearRoot() {
  if (!root_.get())
    return;

  // The root is not yet initialized.
  if (root_->resource_id().empty())
    return;

  // Note that children have a reference to root_,
  // so we need to delete them here.
  root_->RemoveChildren();
  RemoveEntryFromResourceMap(root_->resource_id());
  DCHECK(resource_map_.empty());
  // The resource_map_ should be empty here, but to make sure for non-Debug
  // build.
  resource_map_.clear();
  root_.reset();
}

void DriveResourceMetadata::AddEntryToDirectory(
    const FilePath& directory_path,
    scoped_ptr<google_apis::ResourceEntry> entry,
    const FileMoveCallback& callback) {
  DCHECK(!directory_path.empty());
  DCHECK(!callback.is_null());

  if (!entry.get()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_FAILED);
    return;
  }

  DriveEntry* dir_entry = FindEntryByPathSync(directory_path);
  if (!dir_entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DriveDirectory* directory = dir_entry->AsDriveDirectory();
  if (!directory) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  AddEntryToDirectoryInternal(
      directory,
      ConvertResourceEntryToDriveEntryProto(*entry),
      callback);
}

void DriveResourceMetadata::MoveEntryToDirectory(
    const FilePath& file_path,
    const FilePath& directory_path,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_path.empty());
  DCHECK(!file_path.empty());
  DCHECK(!callback.is_null());

  DriveEntry* entry = FindEntryByPathSync(file_path);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DriveEntry* destination = FindEntryByPathSync(directory_path);
  FilePath moved_file_path;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  if (!destination) {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  } else if (!destination->AsDriveDirectory()) {
    error = DRIVE_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    if (entry->parent())
      entry->parent()->RemoveChild(entry);

    destination->AsDriveDirectory()->AddEntry(entry);
    moved_file_path = entry->GetFilePath();
    error = DRIVE_FILE_OK;
  }
  DVLOG(1) << "MoveEntryToDirectory " << moved_file_path.value();
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, error, moved_file_path));
}

void DriveResourceMetadata::RenameEntry(
  const FilePath& file_path,
  const FilePath::StringType& new_name,
  const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!file_path.empty());
  DCHECK(!new_name.empty());
  DCHECK(!callback.is_null());

  DVLOG(1) << "RenameEntry " << file_path.value() << " to " << new_name;
  DriveEntry* entry = FindEntryByPathSync(file_path);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  if (new_name == file_path.BaseName().value()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_EXISTS);
    return;
  }

  entry->set_title(new_name);
  DCHECK(entry->parent());
  // After changing the title of the entry, call MoveEntryToDirectory to
  // remove the entry from its parent directory and then add it back in order to
  // go through the file name de-duplication.
  // TODO(achuith/satorux/zel): This code is fragile. The title has been
  // changed, but not the file_name. MoveEntryToDirectory calls RemoveChild to
  // remove the child based on the old file_name, and then re-adds the child by
  // first assigning the new title to file_name. http://crbug.com/30157
  MoveEntryToDirectory(file_path, entry->parent()->GetFilePath(), callback);
}

void DriveResourceMetadata::RemoveEntryFromParent(
    const std::string& resource_id,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Disallow deletion of root.
  if (resource_id == root_->resource_id()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_ACCESS_DENIED);
    return;
  }

  DriveEntry* entry = GetEntryByResourceId(resource_id);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DriveDirectory* parent = entry->parent();
  DCHECK(parent);

  DVLOG(1) << "RemoveEntryFromParent " << entry->GetFilePath().value();
  parent->RemoveEntry(entry);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK, parent->GetFilePath()));
}

bool DriveResourceMetadata::AddEntryToResourceMap(DriveEntry* entry) {
  DVLOG(1) << "AddEntryToResourceMap " << entry->resource_id();
  DCHECK(!entry->resource_id().empty());
  std::pair<ResourceMap::iterator, bool> ret =
      resource_map_.insert(std::make_pair(entry->resource_id(), entry));
  DCHECK(ret.second);  // resource_id did not previously exist in the map.
  return ret.second;
}

void DriveResourceMetadata::RemoveEntryFromResourceMap(
    const std::string& resource_id) {
  DVLOG(1) << "RemoveEntryFromResourceMap " << resource_id;
  DCHECK(!resource_id.empty());
  size_t ret = resource_map_.erase(resource_id);
  DCHECK_EQ(1u, ret);  // resource_id was found in the map.
}

DriveEntry* DriveResourceMetadata::FindEntryByPathSync(
    const FilePath& file_path) {
  if (file_path == root_->GetFilePath())
    return root_.get();

  std::vector<FilePath::StringType> components;
  file_path.GetComponents(&components);
  DriveDirectory* current_dir = root_.get();

  for (size_t i = 1; i < components.size() && current_dir; ++i) {
    std::string resource_id = current_dir->FindChild(components[i]);
    if (resource_id.empty())
      return NULL;

    DriveEntry* entry = GetEntryByResourceId(resource_id);
    DCHECK(entry);

    if (i == components.size() - 1)  // Last component.
      return entry;
    else
      current_dir = entry->AsDriveDirectory();
  }
  return NULL;
}

DriveEntry* DriveResourceMetadata::GetEntryByResourceId(
    const std::string& resource_id) {
  DCHECK(!resource_id.empty());
  ResourceMap::const_iterator iter = resource_map_.find(resource_id);
  return iter == resource_map_.end() ? NULL : iter->second;
}

void DriveResourceMetadata::GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> entry_proto;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;

  DriveEntry* entry = GetEntryByResourceId(resource_id);
  if (entry) {
    entry_proto.reset(new DriveEntryProto);
    entry->ToProtoFull(entry_proto.get());
    error = DRIVE_FILE_OK;
    drive_file_path = entry->GetFilePath();
  } else {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, drive_file_path, base::Passed(&entry_proto)));
}

void DriveResourceMetadata::GetEntryInfoByPath(
    const FilePath& path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> entry_proto;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;

  DriveEntry* entry = FindEntryByPathSync(path);
  if (entry) {
    entry_proto.reset(new DriveEntryProto);
    entry->ToProtoFull(entry_proto.get());
    error = DRIVE_FILE_OK;
  } else {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&entry_proto)));
}

void DriveResourceMetadata::ReadDirectoryByPath(
    const FilePath& path,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProtoVector> entries;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;

  DriveEntry* entry = FindEntryByPathSync(path);
  if (entry && entry->AsDriveDirectory()) {
    entries = entry->AsDriveDirectory()->ToProtoVector();
    error = DRIVE_FILE_OK;
  } else if (entry && !entry->AsDriveDirectory()) {
    error = DRIVE_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&entries)));
}

void DriveResourceMetadata::GetEntryInfoPairByPaths(
    const FilePath& first_path,
    const FilePath& second_path,
    const GetEntryInfoPairCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Get the first entry.
  GetEntryInfoByPath(
      first_path,
      base::Bind(&DriveResourceMetadata::GetEntryInfoPairByPathsAfterGetFirst,
                 weak_ptr_factory_.GetWeakPtr(),
                 first_path,
                 second_path,
                 callback));
}

void DriveResourceMetadata::RefreshEntry(
    const DriveEntryProto& entry_proto,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntry> drive_entry = CreateDriveEntryFromProto(entry_proto);
  if (!drive_entry.get()) {
    PostGetEntryInfoWithFilePathCallbackError(
        callback, DRIVE_FILE_ERROR_FAILED);
    return;
  }

  DriveEntry* old_entry = GetEntryByResourceId(drive_entry->resource_id());
  DriveDirectory* old_parent = old_entry ? old_entry->parent() : NULL;
  DriveDirectory* new_parent = GetParent(entry_proto.parent_resource_id());

  scoped_ptr<DriveEntryProto> result_entry_proto(new DriveEntryProto);
  // We special case root here because old_parent of root is null.
  if ((!old_parent || !new_parent) && old_entry != root_.get()) {
    drive_entry->ToProtoFull(result_entry_proto.get());
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   DRIVE_FILE_ERROR_NOT_FOUND,
                   FilePath(),
                   base::Passed(&result_entry_proto)));
    return;
  }

  // Move children over to the new directory from the existing directory.
  if (drive_entry->AsDriveDirectory() && old_entry->AsDriveDirectory()) {
    drive_entry->AsDriveDirectory()->TakeOverEntries(
        old_entry->AsDriveDirectory());
  }

  DriveEntry* new_entry = drive_entry.release();
  if (old_entry == root_.get()) {
    // Replace root.
    root_.reset(new_entry->AsDriveDirectory());
    resource_map_[root_->resource_id()] = root_.get();
  } else {
    // Remove from the old parent and add to the new parent.
    old_parent->RemoveEntry(old_entry);
    new_parent->AddEntry(new_entry);  // Transfers ownership.
  }

  DVLOG(1) << "RefreshEntry " << new_entry->GetFilePath().value();
  // Note that base_name is not the same for new_entry and entry_proto.
  new_entry->ToProtoFull(result_entry_proto.get());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 DRIVE_FILE_OK,
                 new_entry->GetFilePath(),
                 base::Passed(&result_entry_proto)));
}

void DriveResourceMetadata::RefreshDirectory(
    const std::string& directory_resource_id,
    const DriveEntryProtoMap& entry_proto_map,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveEntry* directory_entry = GetEntryByResourceId(directory_resource_id);

  if (!directory_entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DriveDirectory* directory = directory_entry->AsDriveDirectory();
  if (!directory) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  directory->RemoveChildFiles();
  // Add files from entry_proto_map.
  for (DriveEntryProtoMap::const_iterator it = entry_proto_map.begin();
      it != entry_proto_map.end(); ++it) {
    const DriveEntryProto& entry_proto = it->second;
    // Only refresh files.
    if (!entry_proto.file_info().is_directory())
      directory->AddEntry(CreateDriveEntryFromProto(entry_proto).release());
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK, directory->GetFilePath()));
}

void DriveResourceMetadata::AddEntryToParent(
    const DriveEntryProto& entry_proto,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveDirectory* parent = GetParent(entry_proto.parent_resource_id());
  if (!parent) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }
  AddEntryToDirectoryInternal(parent, entry_proto, callback);
}

void DriveResourceMetadata::AddEntryToDirectoryInternal(
    DriveDirectory* directory,
    const DriveEntryProto& entry_proto,
    const FileMoveCallback& callback) {
  scoped_ptr<DriveEntry> new_entry = CreateDriveEntryFromProto(entry_proto);
  if (!new_entry.get()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_FAILED);
    return;
  }

  DriveEntry* added_entry = new_entry.release();
  directory->AddEntry(added_entry);  // Transfers ownership.
  DVLOG(1) << "AddEntryToDirectoryInternal "
           << added_entry->GetFilePath().value();
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK, added_entry->GetFilePath()));
}

DriveDirectory* DriveResourceMetadata::GetParent(
    const std::string& parent_resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (parent_resource_id.empty())
    return root_.get();

  DriveEntry* entry = GetEntryByResourceId(parent_resource_id);
  return entry ? entry->AsDriveDirectory() : NULL;
}

void DriveResourceMetadata::GetChildDirectories(
    const std::string& resource_id,
    const GetChildDirectoriesCallback& changed_dirs_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!changed_dirs_callback.is_null());

  std::set<FilePath> changed_directories;
  DriveEntry* entry = GetEntryByResourceId(resource_id);
  if (entry && entry->AsDriveDirectory())
    entry->AsDriveDirectory()->GetChildDirectoryPaths(&changed_directories);

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(changed_dirs_callback, changed_directories));
}

void DriveResourceMetadata::RemoveAll(const base::Closure& callback) {
  root_->RemoveChildren();
  base::MessageLoopProxy::current()->PostTask(FROM_HERE, callback);
}

void DriveResourceMetadata::InitFromDB(
    const FilePath& db_path,
    base::SequencedTaskRunner* blocking_task_runner,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!db_path.empty());
  DCHECK(blocking_task_runner);
  DCHECK(!callback.is_null());

  if (resource_metadata_db_.get()) {
    callback.Run(DRIVE_FILE_ERROR_IN_USE);
    return;
  }

  blocking_task_runner_ = blocking_task_runner;

  DVLOG(1) << "InitFromDB " << db_path.value();

  CreateDBParams* create_params =
      new CreateDBParams(db_path, blocking_task_runner);
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CreateResourceMetadataDBOnBlockingPool,
                 create_params),
      base::Bind(&DriveResourceMetadata::InitResourceMap,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(create_params),
                 callback));
}

void DriveResourceMetadata::InitResourceMap(
    CreateDBParams* create_params,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(create_params);
  DCHECK(!resource_metadata_db_.get());
  DCHECK(!callback.is_null());

  SerializedMap* serialized_resources = &create_params->serialized_resources;
  resource_metadata_db_ = create_params->db.Pass();
  if (serialized_resources->empty()) {
    callback.Run(DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  // Save root directory resource ID as ClearRoot() resets |root_|.
  std::string saved_root_resource_id = root_->resource_id();
  ClearRoot();

  // Version check.
  int32 version = 0;
  SerializedMap::iterator iter = serialized_resources->find(kDBKeyVersion);
  if (iter == serialized_resources->end() ||
      !base::StringToInt(iter->second, &version) ||
      version != kProtoVersion) {
    callback.Run(DRIVE_FILE_ERROR_FAILED);
    return;
  }
  serialized_resources->erase(iter);

  // Get the largest changestamp.
  iter = serialized_resources->find(kDBKeyLargestChangestamp);
  if (iter == serialized_resources->end() ||
      !base::StringToInt64(iter->second, &largest_changestamp_)) {
    NOTREACHED() << "Could not find/parse largest_changestamp";
    callback.Run(DRIVE_FILE_ERROR_FAILED);
    return;
  } else {
    DVLOG(1) << "InitResourceMap largest_changestamp_" << largest_changestamp_;
    serialized_resources->erase(iter);
  }

  ResourceMap resource_map;
  for (SerializedMap::const_iterator iter = serialized_resources->begin();
      iter != serialized_resources->end(); ++iter) {
    if (iter->first.find(kDBKeyResourceIdPrefix) != 0) {
      NOTREACHED() << "Incorrect prefix for db key " << iter->first;
      continue;
    }

    const std::string resource_id =
        iter->first.substr(strlen(kDBKeyResourceIdPrefix));
    scoped_ptr<DriveEntry> entry =
        CreateDriveEntryFromProtoString(iter->second);
    if (entry.get()) {
      DVLOG(1) << "Inserting resource " << resource_id
               << " into resource_map";
      resource_map.insert(std::make_pair(resource_id, entry.release()));
    } else {
      NOTREACHED() << "Failed to parse DriveEntry for resource " << resource_id;
    }
  }

  // Fix up parent-child relations.
  for (ResourceMap::iterator iter = resource_map.begin();
      iter != resource_map.end(); ++iter) {
    DriveEntry* entry = iter->second;
    ResourceMap::iterator parent_it =
        resource_map.find(entry->parent_resource_id());
    if (parent_it != resource_map.end()) {
      DriveDirectory* parent = parent_it->second->AsDriveDirectory();
      if (parent) {
        DVLOG(1) << "Adding " << entry->resource_id()
                 << " as a child of " << parent->resource_id();
        parent->AddEntry(entry);
      } else {
        NOTREACHED() << "Parent is not a directory " << parent->resource_id();
      }
    } else if (entry->resource_id() == saved_root_resource_id) {
      root_.reset(entry->AsDriveDirectory());
      DCHECK(root_.get());
      AddEntryToResourceMap(root_.get());
    } else {
      NOTREACHED() << "Missing parent id " << entry->parent_resource_id()
                   << " for resource " << entry->resource_id();
    }
  }

  if (!root_.get()) {
    // TODO(achuith): Initialize |root_| before return.
    callback.Run(DRIVE_FILE_ERROR_FAILED);
    return;
  }
  DCHECK_EQ(resource_map.size(), resource_map_.size());
  DCHECK_EQ(resource_map.size(), serialized_resources->size());

  loaded_ = true;

  callback.Run(DRIVE_FILE_OK);
}

void DriveResourceMetadata::SaveToDB() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!blocking_task_runner_ || !resource_metadata_db_.get()) {
    NOTREACHED();
    return;
  }

  size_t serialized_size = 0;
  SerializedMap serialized_resources;
  for (ResourceMap::const_iterator iter = resource_map_.begin();
      iter != resource_map_.end(); ++iter) {
    DriveEntryProto proto;
    iter->second->ToProtoFull(&proto);
    std::string serialized_string;
    const bool ok = proto.SerializeToString(&serialized_string);
    DCHECK(ok);
    if (ok) {
      serialized_resources.insert(
          std::make_pair(std::string(kDBKeyResourceIdPrefix) + iter->first,
                         serialized_string));
      serialized_size += serialized_string.size();
    }
  }

  serialized_resources.insert(std::make_pair(kDBKeyVersion,
      base::IntToString(kProtoVersion)));
  serialized_resources.insert(std::make_pair(kDBKeyLargestChangestamp,
      base::IntToString(largest_changestamp_)));
  set_last_serialized(base::Time::Now());
  set_serialized_size(serialized_size);

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ResourceMetadataDB::Save,
                 base::Unretained(resource_metadata_db_.get()),
                 serialized_resources));
}

void DriveResourceMetadata::SerializeToString(
    std::string* serialized_proto) const {
  DriveRootDirectoryProto proto;
  root_->ToProto(proto.mutable_drive_directory());
  proto.set_largest_changestamp(largest_changestamp_);
  proto.set_version(kProtoVersion);

  const bool ok = proto.SerializeToString(serialized_proto);
  DCHECK(ok);
}

bool DriveResourceMetadata::ParseFromString(
    const std::string& serialized_proto) {
  DriveRootDirectoryProto proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;

  if (proto.version() != kProtoVersion) {
    LOG(ERROR) << "Incompatible proto detected (incompatible version): "
               << proto.version();
    return false;
  }

  root_->FromProto(proto.drive_directory());
  DCHECK_EQ(kWAPIRootDirectoryResourceId, root_->resource_id());

  loaded_ = true;
  largest_changestamp_ = proto.largest_changestamp();

  return true;
}

scoped_ptr<DriveEntry> DriveResourceMetadata::CreateDriveEntryFromProto(
    const DriveEntryProto& entry_proto) {
  scoped_ptr<DriveEntry> entry;
  // TODO(achuith): This method never fails. Add basic sanity checks for
  // resource_id, etc.
  if (entry_proto.file_info().is_directory()) {
    entry = CreateDriveDirectory();
    // Call DriveEntry::FromProto instead of DriveDirectory::FromProto because
    // the proto does not include children.
    entry->FromProto(entry_proto);
  } else {
    scoped_ptr<DriveFile> file(CreateDriveFile());
    // Call DriveFile::FromProto.
    file->FromProto(entry_proto);
    entry = file.Pass();
  }
  return entry.Pass();
}

scoped_ptr<DriveEntry> DriveResourceMetadata::CreateDriveEntryFromProtoString(
    const std::string& serialized_proto) {
  DriveEntryProto entry_proto;
  if (!entry_proto.ParseFromString(serialized_proto))
    return scoped_ptr<DriveEntry>();

  return CreateDriveEntryFromProto(entry_proto);
}

void DriveResourceMetadata::GetEntryInfoPairByPathsAfterGetFirst(
    const FilePath& first_path,
    const FilePath& second_path,
    const GetEntryInfoPairCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<EntryInfoPairResult> result(new EntryInfoPairResult);
  result->first.path = first_path;
  result->first.error = error;
  result->first.proto = entry_proto.Pass();

  // If the first one is not found, don't continue.
  if (error != DRIVE_FILE_OK) {
    callback.Run(result.Pass());
    return;
  }

  // Get the second entry.
  GetEntryInfoByPath(
      second_path,
      base::Bind(&DriveResourceMetadata::GetEntryInfoPairByPathsAfterGetSecond,
                 weak_ptr_factory_.GetWeakPtr(),
                 second_path,
                 callback,
                 base::Passed(&result)));
}

void DriveResourceMetadata::GetEntryInfoPairByPathsAfterGetSecond(
    const FilePath& second_path,
    const GetEntryInfoPairCallback& callback,
    scoped_ptr<EntryInfoPairResult> result,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  result->second.path = second_path;
  result->second.error = error;
  result->second.proto = entry_proto.Pass();

  callback.Run(result.Pass());
}

}  // namespace drive
