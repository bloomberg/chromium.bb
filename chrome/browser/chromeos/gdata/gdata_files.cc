// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_files.h"

#include <leveldb/db.h>
#include <vector>

#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sequenced_task_runner.h"
#include "base/tracked_objects.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"

using content::BrowserThread;

namespace gdata {
namespace {

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

// m: prefix for filesystem metadata db keys, version and largest_changestamp.
// r: prefix for resource id db keys.
const char kDBKeyLargestChangestamp[] = "m:largest_changestamp";
const char kDBKeyVersion[] = "m:version";
const char kDBKeyResourceIdPrefix[] = "r:";

// Extracts resource_id out of edit url.
std::string ExtractResourceId(const GURL& url) {
  return net::UnescapeURLComponent(url.ExtractFileName(),
                                   net::UnescapeRule::URL_SPECIAL_CHARS);
}

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

// GDataEntry class.

GDataEntry::GDataEntry(GDataDirectoryService* directory_service)
    : parent_(NULL),
      directory_service_(directory_service),
      deleted_(false) {
}

GDataEntry::~GDataEntry() {
}

GDataFile* GDataEntry::AsGDataFile() {
  return NULL;
}

GDataDirectory* GDataEntry::AsGDataDirectory() {
  return NULL;
}

void GDataEntry::InitFromDocumentEntry(DocumentEntry* doc) {
  // For regular files, the 'filename' and 'title' attribute in the metadata
  // may be different (e.g. due to rename). To be consistent with the web
  // interface and other client to use the 'title' attribute, instead of
  // 'filename', as the file name in the local snapshot.
  title_ = UTF16ToUTF8(doc->title());
  // SetBaseNameFromTitle() must be called after |title_| is set.
  SetBaseNameFromTitle();

  file_info_.last_modified = doc->updated_time();
  file_info_.last_accessed = doc->updated_time();
  file_info_.creation_time = doc->published_time();

  resource_id_ = doc->resource_id();
  content_url_ = doc->content_url();
  deleted_ = doc->deleted();

  const Link* edit_link = doc->GetLinkByType(Link::EDIT);
  if (edit_link)
    edit_url_ = edit_link->href();

  const Link* parent_link = doc->GetLinkByType(Link::PARENT);
  if (parent_link)
    parent_resource_id_ = ExtractResourceId(parent_link->href());
}

const GDataFile* GDataEntry::AsGDataFileConst() const {
  // cast away const and call the non-const version. This is safe.
  return const_cast<GDataEntry*>(this)->AsGDataFile();
}

const GDataDirectory* GDataEntry::AsGDataDirectoryConst() const {
  // cast away const and call the non-const version. This is safe.
  return const_cast<GDataEntry*>(this)->AsGDataDirectory();
}

FilePath GDataEntry::GetFilePath() const {
  FilePath path;
  if (parent())
    path = parent()->GetFilePath();
  path = path.Append(base_name());
  return path;
}

void GDataEntry::SetParent(GDataDirectory* parent) {
  parent_ = parent;
  parent_resource_id_ = parent ? parent->resource_id() : "";
}

void GDataEntry::SetBaseNameFromTitle() {
  base_name_ = EscapeUtf8FileName(title_);
}

// static
std::string GDataEntry::EscapeUtf8FileName(const std::string& input) {
  std::string output;
  if (ReplaceChars(input, kSlash, std::string(kEscapedSlash), &output))
    return output;

  return input;
}

// static
std::string GDataEntry::UnescapeUtf8FileName(const std::string& input) {
  std::string output = input;
  ReplaceSubstringsAfterOffset(&output, 0, std::string(kEscapedSlash), kSlash);
  return output;
}

// GDataFile class implementation.

GDataFile::GDataFile(GDataDirectoryService* directory_service)
    : GDataEntry(directory_service),
      kind_(DocumentEntry::UNKNOWN),
      is_hosted_document_(false) {
  file_info_.is_directory = false;
}

GDataFile::~GDataFile() {
}

GDataFile* GDataFile::AsGDataFile() {
  return this;
}

void GDataFile::SetBaseNameFromTitle() {
  if (is_hosted_document_) {
    base_name_ = EscapeUtf8FileName(title_ + document_extension_);
  } else {
    GDataEntry::SetBaseNameFromTitle();
  }
}

void GDataFile::InitFromDocumentEntry(DocumentEntry* doc) {
  GDataEntry::InitFromDocumentEntry(doc);

  // Check if this entry is a true file, or...
  if (doc->is_file()) {
    file_info_.size = doc->file_size();
    file_md5_ = doc->file_md5();

    // The resumable-edit-media link should only be present for regular
    // files as hosted documents are not uploadable.
    const Link* upload_link = doc->GetLinkByType(Link::RESUMABLE_EDIT_MEDIA);
    if (upload_link)
      upload_url_ = upload_link->href();
  } else {
    // ... a hosted document.
    // Attach .g<something> extension to hosted documents so we can special
    // case their handling in UI.
    // TODO(zelidrag): Figure out better way how to pass entry info like kind
    // to UI through the File API stack.
    document_extension_ = doc->GetHostedDocumentExtension();
    // We don't know the size of hosted docs and it does not matter since
    // is has no effect on the quota.
    file_info_.size = 0;
  }
  kind_ = doc->kind();
  content_mime_type_ = doc->content_mime_type();
  is_hosted_document_ = doc->is_hosted_document();
  // SetBaseNameFromTitle() must be called after |title_|,
  // |is_hosted_document_| and |document_extension_| are set.
  SetBaseNameFromTitle();

  const Link* thumbnail_link = doc->GetLinkByType(Link::THUMBNAIL);
  if (thumbnail_link)
    thumbnail_url_ = thumbnail_link->href();

  const Link* alternate_link = doc->GetLinkByType(Link::ALTERNATE);
  if (alternate_link)
    alternate_url_ = alternate_link->href();
}

// GDataDirectory class implementation.

GDataDirectory::GDataDirectory(GDataDirectoryService* directory_service)
    : GDataEntry(directory_service) {
  file_info_.is_directory = true;
}

GDataDirectory::~GDataDirectory() {
  RemoveChildren();
}

GDataDirectory* GDataDirectory::AsGDataDirectory() {
  return this;
}

void GDataDirectory::InitFromDocumentEntry(DocumentEntry* doc) {
  GDataEntry::InitFromDocumentEntry(doc);

  const Link* upload_link = doc->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
  if (upload_link)
    upload_url_ = upload_link->href();
}

void GDataDirectory::AddEntry(GDataEntry* entry) {
  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  entry->SetBaseNameFromTitle();

  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int max_modifier = 1;
  FilePath full_file_name(entry->base_name());
  const std::string extension = full_file_name.Extension();
  const std::string file_name = full_file_name.RemoveExtension().value();
  while (FindChild(full_file_name.value())) {
    if (!extension.empty()) {
      full_file_name = FilePath(base::StringPrintf("%s (%d)%s",
                                                   file_name.c_str(),
                                                   ++max_modifier,
                                                   extension.c_str()));
    } else {
      full_file_name = FilePath(base::StringPrintf("%s (%d)",
                                                   file_name.c_str(),
                                                   ++max_modifier));
    }
  }
  entry->set_base_name(full_file_name.value());

  DVLOG(1) << "AddEntry: dir = " << GetFilePath().value()
           << ", file = " + entry->base_name()
           << ", parent resource = " << entry->parent_resource_id()
           << ", resource = " + entry->resource_id();

  // Add entry to resource map.
  if (directory_service_)
    directory_service_->AddEntryToResourceMap(entry);

  // Setup child and parent links.
  AddChild(entry);
  entry->SetParent(this);
}

bool GDataDirectory::TakeOverEntries(GDataDirectory* dir) {
  for (GDataFileCollection::iterator iter = dir->child_files_.begin();
       iter != dir->child_files_.end(); ++iter) {
    AddEntry(iter->second);
  }
  dir->child_files_.clear();

  for (GDataDirectoryCollection::iterator iter =
      dir->child_directories_.begin();
       iter != dir->child_directories_.end(); ++iter) {
    AddEntry(iter->second);
  }
  dir->child_directories_.clear();
  return true;
}

void GDataDirectory::RemoveEntry(GDataEntry* entry) {
  DCHECK(entry);

  RemoveChild(entry);
  delete entry;
}

GDataEntry* GDataDirectory::FindChild(
    const FilePath::StringType& file_name) const {
  GDataFileCollection::const_iterator it = child_files_.find(file_name);
  if (it != child_files_.end())
    return it->second;

  GDataDirectoryCollection::const_iterator itd =
      child_directories_.find(file_name);
  if (itd != child_directories_.end())
    return itd->second;

  return NULL;
}

void GDataDirectory::AddChild(GDataEntry* entry) {
  DCHECK(entry);

  GDataFile* file = entry->AsGDataFile();
  if (file)
    child_files_.insert(std::make_pair(entry->base_name(), file));

  GDataDirectory* directory = entry->AsGDataDirectory();
  if (directory)
    child_directories_.insert(std::make_pair(entry->base_name(), directory));
}

void GDataDirectory::RemoveChild(GDataEntry* entry) {
  DCHECK(entry);

  const std::string& base_name(entry->base_name());
  // entry must be present in this directory.
  DCHECK_EQ(entry, FindChild(base_name));
  // Remove entry from resource map first.
  if (directory_service_)
    directory_service_->RemoveEntryFromResourceMap(entry);

  // Then delete it from tree.
  child_files_.erase(base_name);
  child_directories_.erase(base_name);
}

void GDataDirectory::RemoveChildren() {
  RemoveChildFiles();
  RemoveChildDirectories();
}

void GDataDirectory::RemoveChildFiles() {
  for (GDataFileCollection::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    if (directory_service_)
      directory_service_->RemoveEntryFromResourceMap(iter->second);
  }
  STLDeleteValues(&child_files_);
  child_files_.clear();
}

void GDataDirectory::RemoveChildDirectories() {
  for (GDataDirectoryCollection::iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    GDataDirectory* dir = iter->second;
    // Remove directories recursively.
    dir->RemoveChildren();
    if (directory_service_)
      directory_service_->RemoveEntryFromResourceMap(dir);
  }
  STLDeleteValues(&child_directories_);
  child_directories_.clear();
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

void GDataDirectoryService::MoveEntryToDirectory(
    const FilePath& directory_path,
    GDataEntry* entry,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(entry);

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
  if (!callback.is_null()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, error, moved_file_path));
  }
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

// Convert to/from proto.

// static
void GDataEntry::ConvertProtoToPlatformFileInfo(
    const PlatformFileInfoProto& proto,
    base::PlatformFileInfo* file_info) {
  file_info->size = proto.size();
  file_info->is_directory = proto.is_directory();
  file_info->is_symbolic_link = proto.is_symbolic_link();
  file_info->last_modified = base::Time::FromInternalValue(
      proto.last_modified());
  file_info->last_accessed = base::Time::FromInternalValue(
      proto.last_accessed());
  file_info->creation_time = base::Time::FromInternalValue(
      proto.creation_time());
}

// static
void GDataEntry::ConvertPlatformFileInfoToProto(
    const base::PlatformFileInfo& file_info,
    PlatformFileInfoProto* proto) {
  proto->set_size(file_info.size);
  proto->set_is_directory(file_info.is_directory);
  proto->set_is_symbolic_link(file_info.is_symbolic_link);
  proto->set_last_modified(file_info.last_modified.ToInternalValue());
  proto->set_last_accessed(file_info.last_accessed.ToInternalValue());
  proto->set_creation_time(file_info.creation_time.ToInternalValue());
}

bool GDataEntry::FromProto(const GDataEntryProto& proto) {
  ConvertProtoToPlatformFileInfo(proto.file_info(), &file_info_);

  // Don't copy from proto.base_name() as base_name_ is computed in
  // SetBaseNameFromTitle().
  title_ = proto.title();
  resource_id_ = proto.resource_id();
  parent_resource_id_ = proto.parent_resource_id();
  edit_url_ = GURL(proto.edit_url());
  content_url_ = GURL(proto.content_url());
  upload_url_ = GURL(proto.upload_url());
  SetBaseNameFromTitle();

  // Reject older protobuf that does not contain the upload URL.  This URL is
  // necessary for uploading files.
  if (!proto.has_upload_url()) {
    LOG(ERROR) << "Incompatible proto detected (no upload URL): "
               << proto.title();
    return false;
  }

  return true;
}

void GDataEntry::ToProto(GDataEntryProto* proto) const {
  ConvertPlatformFileInfoToProto(file_info_, proto->mutable_file_info());

  // The base_name field is used in GetFileInfoByPathAsync(). As shown in
  // FromProto(), the value is discarded when deserializing from proto.
  proto->set_base_name(base_name_);
  proto->set_title(title_);
  proto->set_resource_id(resource_id_);
  proto->set_parent_resource_id(parent_resource_id_);
  proto->set_edit_url(edit_url_.spec());
  proto->set_content_url(content_url_.spec());
  proto->set_upload_url(upload_url_.spec());
}

void GDataEntry::ToProtoFull(GDataEntryProto* proto) const {
  if (AsGDataFileConst()) {
    AsGDataFileConst()->ToProto(proto);
  } else if (AsGDataDirectoryConst()) {
    // Unlike files, directories don't have directory specific info, so just
    // calling GDataEntry::ToProto().
    ToProto(proto);
  } else {
    NOTREACHED();
  }
}

bool GDataFile::FromProto(const GDataEntryProto& proto) {
  DCHECK(!proto.file_info().is_directory());

  if (!GDataEntry::FromProto(proto))
    return false;

  thumbnail_url_ = GURL(proto.file_specific_info().thumbnail_url());
  alternate_url_ = GURL(proto.file_specific_info().alternate_url());
  content_mime_type_ = proto.file_specific_info().content_mime_type();
  file_md5_ = proto.file_specific_info().file_md5();
  document_extension_ = proto.file_specific_info().document_extension();
  is_hosted_document_ = proto.file_specific_info().is_hosted_document();

  return true;
}

void GDataFile::ToProto(GDataEntryProto* proto) const {
  GDataEntry::ToProto(proto);
  DCHECK(!proto->file_info().is_directory());
  GDataFileSpecificInfo* file_specific_info =
      proto->mutable_file_specific_info();
  file_specific_info->set_thumbnail_url(thumbnail_url_.spec());
  file_specific_info->set_alternate_url(alternate_url_.spec());
  file_specific_info->set_content_mime_type(content_mime_type_);
  file_specific_info->set_file_md5(file_md5_);
  file_specific_info->set_document_extension(document_extension_);
  file_specific_info->set_is_hosted_document(is_hosted_document_);
}

bool GDataDirectory::FromProto(const GDataDirectoryProto& proto) {
  DCHECK(proto.gdata_entry().file_info().is_directory());
  DCHECK(!proto.gdata_entry().has_file_specific_info());

  for (int i = 0; i < proto.child_files_size(); ++i) {
    scoped_ptr<GDataFile> file(directory_service_->CreateGDataFile());
    if (!file->FromProto(proto.child_files(i))) {
      RemoveChildren();
      return false;
    }
    AddEntry(file.release());
  }
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    scoped_ptr<GDataDirectory> dir(directory_service_->CreateGDataDirectory());
    if (!dir->FromProto(proto.child_directories(i))) {
      RemoveChildren();
      return false;
    }
    AddEntry(dir.release());
  }

  // The states of the directory should be updated after children are
  // handled successfully, so that incomplete states are not left.
  if (!GDataEntry::FromProto(proto.gdata_entry()))
    return false;

  return true;
}

void GDataDirectory::ToProto(GDataDirectoryProto* proto) const {
  GDataEntry::ToProto(proto->mutable_gdata_entry());
  DCHECK(proto->gdata_entry().file_info().is_directory());

  for (GDataFileCollection::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    GDataFile* file = iter->second;
    file->ToProto(proto->add_child_files());
  }
  for (GDataDirectoryCollection::const_iterator iter =
       child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    GDataDirectory* dir = iter->second;
    dir->ToProto(proto->add_child_directories());
  }
}

scoped_ptr<GDataEntryProtoVector> GDataDirectory::ToProtoVector() const {
  scoped_ptr<GDataEntryProtoVector> entries(new GDataEntryProtoVector);
  for (GDataFileCollection::const_iterator iter = child_files().begin();
       iter != child_files().end(); ++iter) {
    GDataEntryProto proto;
    iter->second->ToProto(&proto);
    entries->push_back(proto);
  }
  for (GDataDirectoryCollection::const_iterator iter =
           child_directories().begin();
       iter != child_directories().end(); ++iter) {
    GDataEntryProto proto;
    // Convert to GDataEntry, as we don't want to include children in |proto|.
    static_cast<const GDataEntry*>(iter->second)->ToProtoFull(&proto);
    entries->push_back(proto);
  }

  return entries.Pass();
}

void GDataEntry::SerializeToString(std::string* serialized_proto) const {
  const GDataFile* file = AsGDataFileConst();
  const GDataDirectory* dir = AsGDataDirectoryConst();

  if (file) {
    GDataEntryProto entry_proto;
    file->ToProto(&entry_proto);
    const bool ok = entry_proto.SerializeToString(serialized_proto);
    DCHECK(ok);
  } else if (dir) {
    GDataDirectoryProto dir_proto;
    dir->ToProto(&dir_proto);
    const bool ok = dir_proto.SerializeToString(serialized_proto);
    DCHECK(ok);
  }
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
