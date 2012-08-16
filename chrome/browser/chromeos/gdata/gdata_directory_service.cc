// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_directory_service.h"

#include <leveldb/db.h>

#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/sequenced_task_runner.h"
#include "base/tracked_objects.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {
namespace {

// m: prefix for filesystem metadata db keys, version and largest_changestamp.
// r: prefix for resource id db keys.
const char kDBKeyLargestChangestamp[] = "m:largest_changestamp";
const char kDBKeyVersion[] = "m:version";
const char kDBKeyResourceIdPrefix[] = "r:";

// Returns true if |proto| is a valid proto as the root directory.
// Used to reject incompatible proto.
bool IsValidRootDirectoryProto(const GDataDirectoryProto& proto) {
  const GDataEntryProto& entry_proto = proto.gdata_entry();
  // The title field for the root directory was originally empty, then
  // changed to "gdata", then changed to "drive". Discard the proto data if
  // the older formats are detected. See crbug.com/128133 for details.
  if (entry_proto.title() != "drive") {
    LOG(ERROR) << "Incompatible proto detected (bad title): "
               << entry_proto.title();
    return false;
  }
  // The title field for the root directory was originally empty. Discard
  // the proto data if the older format is detected.
  if (entry_proto.resource_id() != kGDataRootDirectoryResourceId) {
    LOG(ERROR) << "Incompatible proto detected (bad resource ID): "
               << entry_proto.resource_id();
    return false;
  }

  return true;
}

}  // namespace

EntryInfoResult::EntryInfoResult() : error(GDATA_FILE_ERROR_FAILED) {
}

EntryInfoResult::~EntryInfoResult() {
}

EntryInfoPairResult::EntryInfoPairResult() {
}

EntryInfoPairResult::~EntryInfoPairResult() {
}

// ResourceMetadataDB implementation.

// Params for GDatadirectoryServiceDB::Create.
struct CreateDBParams {
  CreateDBParams(const FilePath& db_path,
                 base::SequencedTaskRunner* blocking_task_runner)
                 : db_path(db_path),
                   blocking_task_runner(blocking_task_runner) {
  }

  FilePath db_path;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner;
  scoped_ptr<ResourceMetadataDB> db;
  GDataDirectoryService::SerializedMap serialized_resources;
};

// Wrapper for level db. All methods must be called on blocking thread.
class ResourceMetadataDB {
 public:
  ResourceMetadataDB(const FilePath& db_path,
                          base::SequencedTaskRunner* blocking_task_runner);

  // Initializes the database.
  void Init();

  // Reads the database into |serialized_resources|.
  void Read(GDataDirectoryService::SerializedMap* serialized_resources);

  // Saves |serialized_resources| to the database.
  void Save(const GDataDirectoryService::SerializedMap& serialized_resources);

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
  GDataDirectoryService::SerializedMap* serialized_resources) {
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
    const GDataDirectoryService::SerializedMap& serialized_resources) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  Clear();
  for (GDataDirectoryService::SerializedMap::const_iterator iter =
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

// GDataDirectoryService class implementation.

GDataDirectoryService::GDataDirectoryService()
    : blocking_task_runner_(NULL),
      serialized_size_(0),
      largest_changestamp_(0),
      origin_(UNINITIALIZED),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  root_.reset(CreateGDataDirectory());
  if (!util::IsDriveV2ApiEnabled())
    InitializeRootEntry(kGDataRootDirectoryResourceId);
}

GDataDirectoryService::~GDataDirectoryService() {
  ClearRoot();

  // Ensure db is closed on the blocking pool.
  if (blocking_task_runner_ && directory_service_db_.get())
    blocking_task_runner_->DeleteSoon(FROM_HERE,
                                      directory_service_db_.release());
}

GDataEntry* GDataDirectoryService::FromDocumentEntry(DocumentEntry* doc) {
  DCHECK(doc);
  GDataEntry* entry = NULL;
  if (doc->is_folder())
    entry = CreateGDataDirectory();
  else if (doc->is_hosted_document() || doc->is_file())
    entry = CreateGDataFile();

  if (entry)
    entry->InitFromDocumentEntry(doc);
  return entry;
}

GDataFile* GDataDirectoryService::CreateGDataFile() {
  return new GDataFile(this);
}

GDataDirectory* GDataDirectoryService::CreateGDataDirectory() {
  return new GDataDirectory(this);
}

void GDataDirectoryService::InitializeRootEntry(const std::string& root_id) {
  root_.reset(CreateGDataDirectory());
  root_->set_title(kGDataRootDirectory);
  root_->SetBaseNameFromTitle();
  root_->set_resource_id(root_id);
  AddEntryToResourceMap(root_.get());
}

void GDataDirectoryService::ClearRoot() {
  // Note that children have a reference to root_,
  // so we need to delete them here.
  root_->RemoveChildren();
  RemoveEntryFromResourceMap(root_.get());
  DCHECK(resource_map_.empty());
  resource_map_.clear();
  root_.reset();
}

void GDataDirectoryService::AddEntryToDirectory(
    GDataDirectory* directory,
    GDataEntry* new_entry,
    const FileMoveCallback& callback) {
  DCHECK(directory);
  DCHECK(new_entry);
  DCHECK(!callback.is_null());

  directory->AddEntry(new_entry);
  DVLOG(1) << "AddEntryToDirectory " << new_entry->GetFilePath().value();
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(callback, GDATA_FILE_OK, new_entry->GetFilePath()));
}

void GDataDirectoryService::MoveEntryToDirectory(
    const FilePath& directory_path,
    GDataEntry* entry,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(entry);
  DCHECK(!callback.is_null());

  if (entry->parent())
    entry->parent()->RemoveChild(entry);

  GDataEntry* destination = FindEntryByPathSync(directory_path);
  FilePath moved_file_path;
  GDataFileError error = GDATA_FILE_ERROR_FAILED;
  if (!destination) {
    error = GDATA_FILE_ERROR_NOT_FOUND;
  } else if (!destination->AsGDataDirectory()) {
    error = GDATA_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    destination->AsGDataDirectory()->AddEntry(entry);
    moved_file_path = entry->GetFilePath();
    error = GDATA_FILE_OK;
  }
  DVLOG(1) << "MoveEntryToDirectory " << moved_file_path.value();
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, error, moved_file_path));
}

void GDataDirectoryService::RemoveEntryFromParent(
    GDataEntry* entry,
    const FileMoveCallback& callback) {
  GDataDirectory* parent = entry->parent();
  DCHECK(parent);
  DCHECK(!callback.is_null());
  DVLOG(1) << "RemoveEntryFromParent " << entry->GetFilePath().value();

  parent->RemoveEntry(entry);
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(callback, GDATA_FILE_OK, parent->GetFilePath()));
}

void GDataDirectoryService::AddEntryToResourceMap(GDataEntry* entry) {
  // GDataFileSystem has already locked.
  DVLOG(1) << "AddEntryToResourceMap " << entry->resource_id();
  resource_map_.insert(std::make_pair(entry->resource_id(), entry));
}

void GDataDirectoryService::RemoveEntryFromResourceMap(GDataEntry* entry) {
  // GDataFileSystem has already locked.
  resource_map_.erase(entry->resource_id());
}

GDataEntry* GDataDirectoryService::FindEntryByPathSync(
    const FilePath& file_path) {
  if (file_path == root_->GetFilePath())
    return root_.get();

  std::vector<FilePath::StringType> components;
  file_path.GetComponents(&components);
  GDataDirectory* current_dir = root_.get();

  for (size_t i = 1; i < components.size() && current_dir; ++i) {
    GDataEntry* entry = current_dir->FindChild(components[i]);
    if (!entry)
      return NULL;

    if (i == components.size() - 1)  // Last component.
      return entry;
    else
      current_dir = entry->AsGDataDirectory();
  }
  return NULL;
}

void GDataDirectoryService::FindEntryByPathAndRunSync(
    const FilePath& search_file_path,
    const FindEntryCallback& callback) {
  GDataEntry* entry = FindEntryByPathSync(search_file_path);
  callback.Run(entry ? GDATA_FILE_OK : GDATA_FILE_ERROR_NOT_FOUND, entry);
}

GDataEntry* GDataDirectoryService::GetEntryByResourceId(
    const std::string& resource) {
  // GDataFileSystem has already locked.
  ResourceMap::const_iterator iter = resource_map_.find(resource);
  return iter == resource_map_.end() ? NULL : iter->second;
}

void GDataDirectoryService::GetEntryByResourceIdAsync(
    const std::string& resource_id,
    const GetEntryByResourceIdCallback& callback) {
  GDataEntry* entry = GetEntryByResourceId(resource_id);
  callback.Run(entry);
}

void GDataDirectoryService::GetEntryInfoByPath(
    const FilePath& path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<GDataEntryProto> entry_proto;
  GDataFileError error = GDATA_FILE_ERROR_FAILED;

  GDataEntry* entry = FindEntryByPathSync(path);
  if (entry) {
    entry_proto.reset(new GDataEntryProto);
    entry->ToProtoFull(entry_proto.get());
    error = GDATA_FILE_OK;
  } else {
    error = GDATA_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&entry_proto)));
}

void GDataDirectoryService::ReadDirectoryByPath(
    const FilePath& path,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<GDataEntryProtoVector> entries;
  GDataFileError error = GDATA_FILE_ERROR_FAILED;

  GDataEntry* entry = FindEntryByPathSync(path);
  if (entry && entry->AsGDataDirectory()) {
    entries = entry->AsGDataDirectory()->ToProtoVector();
    error = GDATA_FILE_OK;
  } else if (entry && !entry->AsGDataDirectory()) {
    error = GDATA_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    error = GDATA_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&entries)));
}

void GDataDirectoryService::GetEntryInfoPairByPaths(
    const FilePath& first_path,
    const FilePath& second_path,
    const GetEntryInfoPairCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Get the first entry.
  GetEntryInfoByPath(
      first_path,
      base::Bind(&GDataDirectoryService::GetEntryInfoPairByPathsAfterGetFirst,
                 weak_ptr_factory_.GetWeakPtr(),
                 first_path,
                 second_path,
                 callback));
}

void GDataDirectoryService::RefreshFile(scoped_ptr<GDataFile> fresh_file) {
  DCHECK(fresh_file.get());

  // Need to get a reference here because Passed() could get evaluated first.
  const std::string& resource_id = fresh_file->resource_id();
  GetEntryByResourceIdAsync(
      resource_id,
      base::Bind(&GDataDirectoryService::RefreshFileInternal,
                 base::Passed(&fresh_file)));
}

// static
void GDataDirectoryService::RefreshFileInternal(
    scoped_ptr<GDataFile> fresh_file,
    GDataEntry* old_entry) {
  GDataDirectory* entry_parent = old_entry ? old_entry->parent() : NULL;
  if (entry_parent) {
    DCHECK_EQ(fresh_file->resource_id(), old_entry->resource_id());
    DCHECK(old_entry->AsGDataFile());

    entry_parent->RemoveEntry(old_entry);
    entry_parent->AddEntry(fresh_file.release());
  }
}

void GDataDirectoryService::RefreshDirectory(
    const std::string& directory_resource_id,
    const ResourceMap& file_map,
    const FileMoveCallback& callback) {
  DCHECK(!callback.is_null());
  GetEntryByResourceIdAsync(
      directory_resource_id,
      base::Bind(&GDataDirectoryService::RefreshDirectoryInternal,
                 file_map,
                 callback));
}

// static
void GDataDirectoryService::RefreshDirectoryInternal(
    const ResourceMap& file_map,
    const FileMoveCallback& callback,
    GDataEntry* directory_entry) {
  DCHECK(!callback.is_null());

  if (!directory_entry) {
    callback.Run(GDATA_FILE_ERROR_NOT_FOUND, FilePath());
    return;
  }

  GDataDirectory* directory = directory_entry->AsGDataDirectory();
  if (!directory) {
    callback.Run(GDATA_FILE_ERROR_NOT_A_DIRECTORY, FilePath());
    return;
  }

  directory->RemoveChildFiles();
  // Add files from file_map.
  for (ResourceMap::const_iterator it = file_map.begin();
       it != file_map.end(); ++it) {
    scoped_ptr<GDataEntry> entry(it->second);
    // Skip if it's not a file (i.e. directory).
    if (!entry->AsGDataFile())
      continue;
    directory->AddEntry(entry.release());
  }

  callback.Run(GDATA_FILE_OK, directory->GetFilePath());
}

void GDataDirectoryService::InitFromDB(
    const FilePath& db_path,
    base::SequencedTaskRunner* blocking_task_runner,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!db_path.empty());
  DCHECK(blocking_task_runner);

  if (directory_service_db_.get()) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_FAILED);
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
      base::Bind(&GDataDirectoryService::InitResourceMap,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(create_params),
                 callback));
}

void GDataDirectoryService::InitResourceMap(
    CreateDBParams* create_params,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(create_params);
  DCHECK(!directory_service_db_.get());

  SerializedMap* serialized_resources = &create_params->serialized_resources;
  directory_service_db_ = create_params->db.Pass();
  if (serialized_resources->empty()) {
    origin_ = INITIALIZING;
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_NOT_FOUND);
    return;
  }

  ClearRoot();

  // Version check.
  int32 version = 0;
  SerializedMap::iterator iter = serialized_resources->find(kDBKeyVersion);
  if (iter == serialized_resources->end() ||
      !base::StringToInt(iter->second, &version) ||
      version != kProtoVersion) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_FAILED);
    return;
  }
  serialized_resources->erase(iter);

  // Get the largest changestamp.
  iter = serialized_resources->find(kDBKeyLargestChangestamp);
  if (iter == serialized_resources->end() ||
      !base::StringToInt64(iter->second, &largest_changestamp_)) {
    NOTREACHED() << "Could not find/parse largest_changestamp";
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_FAILED);
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
    scoped_ptr<GDataEntry> entry = FromProtoString(iter->second);
    if (entry.get()) {
      DVLOG(1) << "Inserting resource " << resource_id
               << " into resource_map";
      resource_map.insert(std::make_pair(resource_id, entry.release()));
    } else {
      NOTREACHED() << "Failed to parse GDataEntry for resource " << resource_id;
    }
  }

  // Fix up parent-child relations.
  for (ResourceMap::iterator iter = resource_map.begin();
      iter != resource_map.end(); ++iter) {
    GDataEntry* entry = iter->second;
    ResourceMap::iterator parent_it =
        resource_map.find(entry->parent_resource_id());
    if (parent_it != resource_map.end()) {
      GDataDirectory* parent = parent_it->second->AsGDataDirectory();
      if (parent) {
        DVLOG(1) << "Adding " << entry->resource_id()
                 << " as a child of " << parent->resource_id();
        parent->AddEntry(entry);
      } else {
        NOTREACHED() << "Parent is not a directory " << parent->resource_id();
      }
    } else if (entry->resource_id() == kGDataRootDirectoryResourceId) {
      root_.reset(entry->AsGDataDirectory());
      DCHECK(root_.get());
      AddEntryToResourceMap(root_.get());
    } else {
      NOTREACHED() << "Missing parent id " << entry->parent_resource_id()
                   << " for resource " << entry->resource_id();
    }
  }

  DCHECK(root_.get());
  DCHECK_EQ(resource_map.size(), resource_map_.size());
  DCHECK_EQ(resource_map.size(), serialized_resources->size());

  origin_ = FROM_CACHE;

  if (!callback.is_null())
    callback.Run(GDATA_FILE_OK);
}

void GDataDirectoryService::SaveToDB() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!blocking_task_runner_ || !directory_service_db_.get()) {
    NOTREACHED();
    return;
  }

  size_t serialized_size = 0;
  SerializedMap serialized_resources;
  for (ResourceMap::const_iterator iter = resource_map_.begin();
      iter != resource_map_.end(); ++iter) {
    GDataEntryProto proto;
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
                 base::Unretained(directory_service_db_.get()),
                 serialized_resources));
}

void GDataDirectoryService::SerializeToString(
    std::string* serialized_proto) const {
  GDataRootDirectoryProto proto;
  root_->ToProto(proto.mutable_gdata_directory());
  proto.set_largest_changestamp(largest_changestamp_);
  proto.set_version(kProtoVersion);

  const bool ok = proto.SerializeToString(serialized_proto);
  DCHECK(ok);
}

bool GDataDirectoryService::ParseFromString(
    const std::string& serialized_proto) {
  GDataRootDirectoryProto proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;

  if (proto.version() != kProtoVersion) {
    LOG(ERROR) << "Incompatible proto detected (incompatible version): "
               << proto.version();
    return false;
  }

  if (!IsValidRootDirectoryProto(proto.gdata_directory()))
    return false;

  if (!root_->FromProto(proto.gdata_directory()))
    return false;

  origin_ = FROM_CACHE;
  largest_changestamp_ = proto.largest_changestamp();

  return true;
}

scoped_ptr<GDataEntry> GDataDirectoryService::FromProtoString(
    const std::string& serialized_proto) {
  GDataEntryProto entry_proto;
  if (!entry_proto.ParseFromString(serialized_proto))
    return scoped_ptr<GDataEntry>();

  scoped_ptr<GDataEntry> entry;
  if (entry_proto.file_info().is_directory()) {
    entry.reset(CreateGDataDirectory());
    // Call GDataEntry::FromProto instead of GDataDirectory::FromProto because
    // the proto does not include children.
    if (!entry->FromProto(entry_proto)) {
      NOTREACHED() << "FromProto (directory) failed";
      entry.reset();
    }
  } else {
    scoped_ptr<GDataFile> file(CreateGDataFile());
    // Call GDataFile::FromProto.
    if (file->FromProto(entry_proto)) {
      entry.reset(file.release());
    } else {
      NOTREACHED() << "FromProto (file) failed";
    }
  }
  return entry.Pass();
}

void GDataDirectoryService::GetEntryInfoPairByPathsAfterGetFirst(
    const FilePath& first_path,
    const FilePath& second_path,
    const GetEntryInfoPairCallback& callback,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<EntryInfoPairResult> result(new EntryInfoPairResult);
  result->first.path = first_path;
  result->first.error = error;
  result->first.proto = entry_proto.Pass();

  // If the first one is not found, don't continue.
  if (error != GDATA_FILE_OK) {
    callback.Run(result.Pass());
    return;
  }

  // Get the second entry.
  GetEntryInfoByPath(
      second_path,
      base::Bind(&GDataDirectoryService::GetEntryInfoPairByPathsAfterGetSecond,
                 weak_ptr_factory_.GetWeakPtr(),
                 second_path,
                 callback,
                 base::Passed(&result)));
}

void GDataDirectoryService::GetEntryInfoPairByPathsAfterGetSecond(
    const FilePath& second_path,
    const GetEntryInfoPairCallback& callback,
    scoped_ptr<EntryInfoPairResult> result,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  result->second.path = second_path;
  result->second.error = error;
  result->second.proto = entry_proto.Pass();

  callback.Run(result.Pass());
}

}  // namespace gdata
