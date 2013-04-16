// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"

#include "base/file_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata_storage.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace {

// Sets entry's base name from its title and other attributes.
void SetBaseNameFromTitle(DriveEntryProto* entry) {
  std::string base_name = entry->title();
  if (entry->has_file_specific_info() &&
      entry->file_specific_info().is_hosted_document()) {
    base_name += entry->file_specific_info().document_extension();
  }
  entry->set_base_name(util::EscapeUtf8FileName(base_name));
}

// Creates an entry by copying |source|, and setting the base name properly.
DriveEntryProto CreateEntryWithProperBaseName(const DriveEntryProto& source) {
  DriveEntryProto entry(source);
  SetBaseNameFromTitle(&entry);
  return entry;
}

// Runs |callback| with |result|. Used to implement GetChildDirectories().
void RunGetChildDirectoriesCallbackWithResult(
    const GetChildDirectoriesCallback& callback,
    scoped_ptr<std::set<base::FilePath> > result) {
  DCHECK(!callback.is_null());
  callback.Run(*result);
}

}  // namespace

std::string DirectoryFetchInfo::ToString() const {
  return ("resource_id: " + resource_id_ +
          ", changestamp: " + base::Int64ToString(changestamp_));
}

EntryInfoResult::EntryInfoResult() : error(DRIVE_FILE_ERROR_FAILED) {
}

EntryInfoResult::~EntryInfoResult() {
}

EntryInfoPairResult::EntryInfoPairResult() {
}

EntryInfoPairResult::~EntryInfoPairResult() {
}

// Struct to hold result values passed to FileMoveCallback.
struct DriveResourceMetadata::FileMoveResult {
  FileMoveResult(DriveFileError error, const base::FilePath& path)
      : error(error),
        path(path) {}

  explicit FileMoveResult(DriveFileError error)
      : error(error) {}

  FileMoveResult() : error(DRIVE_FILE_OK) {}

  // Runs GetEntryInfoCallback with the values stored in |result|.
  static void RunCallbackWithResult(const FileMoveCallback& callback,
                                    const FileMoveResult& result) {
    DCHECK(!callback.is_null());
    callback.Run(result.error, result.path);
  }

  DriveFileError error;
  base::FilePath path;
};

// Struct to hold result values passed to GetEntryInfoCallback.
struct DriveResourceMetadata::GetEntryInfoResult {
  GetEntryInfoResult(DriveFileError error, scoped_ptr<DriveEntryProto> entry)
      : error(error),
        entry(entry.Pass()) {}

  explicit GetEntryInfoResult(DriveFileError error)
      : error(error) {}

  // Runs GetEntryInfoCallback with the values stored in |result|.
  static void RunCallbackWithResult(const GetEntryInfoCallback& callback,
                                    scoped_ptr<GetEntryInfoResult> result) {
    DCHECK(!callback.is_null());
    DCHECK(result);
    callback.Run(result->error, result->entry.Pass());
  }

  DriveFileError error;
  scoped_ptr<DriveEntryProto> entry;
};

// Struct to hold result values passed to GetEntryInfoWithFilePathCallback.
struct DriveResourceMetadata::GetEntryInfoWithFilePathResult {
  GetEntryInfoWithFilePathResult(DriveFileError error,
                                 const base::FilePath& path,
                                 scoped_ptr<DriveEntryProto> entry)
      : error(error),
        path(path),
        entry(entry.Pass()) {}

  explicit GetEntryInfoWithFilePathResult(DriveFileError error)
      : error(error) {}

  // Runs GetEntryInfoWithFilePathCallback with the values stored in |result|.
  static void RunCallbackWithResult(
      const GetEntryInfoWithFilePathCallback& callback,
      scoped_ptr<GetEntryInfoWithFilePathResult> result) {
    DCHECK(!callback.is_null());
    DCHECK(result);
    callback.Run(result->error, result->path, result->entry.Pass());
  }

  DriveFileError error;
  base::FilePath path;
  scoped_ptr<DriveEntryProto> entry;
};

// Struct to hold result values passed to ReadDirectoryCallback.
struct DriveResourceMetadata::ReadDirectoryResult {
  ReadDirectoryResult(DriveFileError error,
                      scoped_ptr<DriveEntryProtoVector> entries)
      : error(error),
        entries(entries.Pass()) {}

  // Runs ReadDirectoryCallback with the values stored in |result|.
  static void RunCallbackWithResult(const ReadDirectoryCallback& callback,
                                    scoped_ptr<ReadDirectoryResult> result) {
    DCHECK(!callback.is_null());
    DCHECK(result);
    callback.Run(result->error, result->entries.Pass());
  }

  DriveFileError error;
  scoped_ptr<DriveEntryProtoVector> entries;
};

// DriveResourceMetadata class implementation.

DriveResourceMetadata::DriveResourceMetadata(
    const base::FilePath& data_directory_path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : data_directory_path_(data_directory_path),
      blocking_task_runner_(blocking_task_runner),
      storage_(new DriveResourceMetadataStorageDB(data_directory_path)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DriveResourceMetadata::Initialize(const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::InitializeOnBlockingPool,
                 base::Unretained(this)),
      callback);
}

void DriveResourceMetadata::Destroy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  weak_ptr_factory_.InvalidateWeakPtrs();
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::DestroyOnBlockingPool,
                 base::Unretained(this)));
}

void DriveResourceMetadata::Reset(const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::ResetOnBlockingPool,
                 base::Unretained(this)),
      callback);
}

DriveResourceMetadata::~DriveResourceMetadata() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
}

DriveFileError DriveResourceMetadata::InitializeOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  // Initialize the storage.
  if (!storage_->Initialize())
    return DRIVE_FILE_ERROR_FAILED;

  SetUpDefaultEntries();

  return DRIVE_FILE_OK;
}

void DriveResourceMetadata::SetUpDefaultEntries() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  // Initialize the grand root and "other" entries. "/drive" and "/drive/other".
  // As an intermediate change, "/drive/root" is also added here.
  if (!storage_->GetEntry(util::kDriveGrandRootSpecialResourceId)) {
    DriveEntryProto root;
    root.mutable_file_info()->set_is_directory(true);
    root.set_resource_id(util::kDriveGrandRootSpecialResourceId);
    root.set_title(util::kDriveGrandRootDirName);
    storage_->PutEntry(CreateEntryWithProperBaseName(root));
  }
  if (!storage_->GetEntry(util::kDriveOtherDirSpecialResourceId)) {
    AddEntryToDirectory(util::CreateOtherDirEntry());
  }
}

void DriveResourceMetadata::DestroyOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  delete this;
}

void DriveResourceMetadata::ResetOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  RemoveAllOnBlockingPool();
  storage_->SetLargestChangestamp(0);
}

void DriveResourceMetadata::GetLargestChangestamp(
    const GetChangestampCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::GetLargestChangestampOnBlockingPool,
                 base::Unretained(this)),
      callback);
}

void DriveResourceMetadata::SetLargestChangestamp(
    int64 value,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::SetLargestChangestampOnBlockingPool,
                 base::Unretained(this),
                 value),
      callback);
}

void DriveResourceMetadata::AddEntry(const DriveEntryProto& entry_proto,
                                     const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::AddEntryOnBlockingPool,
                 base::Unretained(this),
                 entry_proto),
      base::Bind(&FileMoveResult::RunCallbackWithResult,
                 callback));
}

void DriveResourceMetadata::MoveEntryToDirectory(
    const base::FilePath& file_path,
    const base::FilePath& directory_path,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::MoveEntryToDirectoryOnBlockingPool,
                 base::Unretained(this),
                 file_path,
                 directory_path),
      base::Bind(&FileMoveResult::RunCallbackWithResult,
                 callback));
}

void DriveResourceMetadata::RenameEntry(const base::FilePath& file_path,
                                        const std::string& new_name,
                                        const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::RenameEntryOnBlockingPool,
                 base::Unretained(this),
                 file_path,
                 new_name),
      base::Bind(&FileMoveResult::RunCallbackWithResult,
                 callback));
}

void DriveResourceMetadata::RemoveEntry(const std::string& resource_id,
                                        const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::RemoveEntryOnBlockingPool,
                 base::Unretained(this),
                 resource_id),
      base::Bind(&FileMoveResult::RunCallbackWithResult,
                 callback));
}

void DriveResourceMetadata::GetEntryInfoByResourceId(
    const std::string& resource_id,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::GetEntryInfoByResourceIdOnBlockingPool,
                 base::Unretained(this),
                 resource_id),
      base::Bind(&GetEntryInfoWithFilePathResult::RunCallbackWithResult,
                 callback));
}

void DriveResourceMetadata::GetEntryInfoByPath(
    const base::FilePath& file_path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::GetEntryInfoByPathOnBlockingPool,
                 base::Unretained(this),
                 file_path),
      base::Bind(&GetEntryInfoResult::RunCallbackWithResult,
                 callback));
}

void DriveResourceMetadata::ReadDirectoryByPath(
    const base::FilePath& file_path,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::ReadDirectoryByPathOnBlockingPool,
                 base::Unretained(this),
                 file_path),
      base::Bind(&ReadDirectoryResult::RunCallbackWithResult,
                 callback));
}

void DriveResourceMetadata::RefreshEntry(
    const DriveEntryProto& entry_proto,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::RefreshEntryOnBlockingPool,
                 base::Unretained(this),
                 entry_proto),
      base::Bind(&GetEntryInfoWithFilePathResult::RunCallbackWithResult,
                 callback));
}

void DriveResourceMetadata::RefreshDirectory(
    const DirectoryFetchInfo& directory_fetch_info,
    const DriveEntryProtoMap& entry_proto_map,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::RefreshDirectoryOnBlockingPool,
                 base::Unretained(this),
                 directory_fetch_info,
                 entry_proto_map),
      base::Bind(&FileMoveResult::RunCallbackWithResult,
                 callback));
}

void DriveResourceMetadata::GetChildDirectories(
    const std::string& resource_id,
    const GetChildDirectoriesCallback& changed_dirs_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!changed_dirs_callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::GetChildDirectoriesOnBlockingPool,
                 base::Unretained(this),
                 resource_id),
      base::Bind(&RunGetChildDirectoriesCallbackWithResult,
                 changed_dirs_callback));
}

void DriveResourceMetadata::RemoveAll(const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::RemoveAllOnBlockingPool,
                 base::Unretained(this)),
      callback);
}

void DriveResourceMetadata::IterateEntries(
    const IterateCallback& iterate_callback,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!iterate_callback.is_null());
  DCHECK(!completion_callback.is_null());

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DriveResourceMetadata::IterateEntriesOnBlockingPool,
                 base::Unretained(this),
                 iterate_callback),
      completion_callback);
}

int64 DriveResourceMetadata::GetLargestChangestampOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  return storage_->GetLargestChangestamp();
}

DriveFileError DriveResourceMetadata::SetLargestChangestampOnBlockingPool(
    int64 value) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  storage_->SetLargestChangestamp(value);
  return DRIVE_FILE_OK;
}

DriveResourceMetadata::FileMoveResult
DriveResourceMetadata::MoveEntryToDirectoryOnBlockingPool(
    const base::FilePath& file_path,
    const base::FilePath& directory_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!directory_path.empty());
  DCHECK(!file_path.empty());

  scoped_ptr<DriveEntryProto> entry = FindEntryByPathSync(file_path);
  if (!entry)
    return FileMoveResult(DRIVE_FILE_ERROR_NOT_FOUND);

  // Cannot move an entry without its parent. (i.e. the root)
  if (entry->parent_resource_id().empty())
    return FileMoveResult(DRIVE_FILE_ERROR_INVALID_OPERATION);

  scoped_ptr<DriveEntryProto> destination = FindEntryByPathSync(directory_path);
  base::FilePath moved_file_path;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  if (!destination) {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  } else if (!destination->file_info().is_directory()) {
    error = DRIVE_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    DetachEntryFromDirectory(entry->resource_id());
    entry->set_parent_resource_id(destination->resource_id());
    AddEntryToDirectory(*entry);
    moved_file_path = GetFilePath(entry->resource_id());
    error = DRIVE_FILE_OK;
  }
  DVLOG(1) << "MoveEntryToDirectory " << moved_file_path.value();
  return FileMoveResult(error, moved_file_path);
}

DriveResourceMetadata::FileMoveResult
DriveResourceMetadata::RenameEntryOnBlockingPool(
    const base::FilePath& file_path,
    const std::string& new_name) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!file_path.empty());
  DCHECK(!new_name.empty());

  DVLOG(1) << "RenameEntry " << file_path.value() << " to " << new_name;
  scoped_ptr<DriveEntryProto> entry = FindEntryByPathSync(file_path);
  if (!entry)
    return FileMoveResult(DRIVE_FILE_ERROR_NOT_FOUND);

  if (base::FilePath::FromUTF8Unsafe(new_name) == file_path.BaseName())
    return FileMoveResult(DRIVE_FILE_ERROR_EXISTS);

  entry->set_title(new_name);
  scoped_ptr<GetEntryInfoWithFilePathResult> result =
      RefreshEntryOnBlockingPool(*entry);
  return FileMoveResult(result->error, result->path);
}

DriveResourceMetadata::FileMoveResult
DriveResourceMetadata::RemoveEntryOnBlockingPool(
    const std::string& resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  // Disallow deletion of special entries "/drive" and "/drive/other".
  if (util::IsSpecialResourceId(resource_id))
    return FileMoveResult(DRIVE_FILE_ERROR_ACCESS_DENIED);

  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  if (!entry)
    return FileMoveResult(DRIVE_FILE_ERROR_NOT_FOUND);

  RemoveDirectoryChild(entry->resource_id());
  return FileMoveResult(DRIVE_FILE_OK,
                        GetFilePath(entry->parent_resource_id()));
}

scoped_ptr<DriveEntryProto> DriveResourceMetadata::FindEntryByPathSync(
    const base::FilePath& file_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  // Start from the root.
  scoped_ptr<DriveEntryProto> entry =
      storage_->GetEntry(util::kDriveGrandRootSpecialResourceId);
  DCHECK(entry);
  DCHECK(entry->parent_resource_id().empty());

  // Check the first component.
  std::vector<base::FilePath::StringType> components;
  file_path.GetComponents(&components);
  if (components.empty() ||
      base::FilePath(components[0]).AsUTF8Unsafe() != entry->base_name())
    return scoped_ptr<DriveEntryProto>();

  // Iterate over the remaining components.
  for (size_t i = 1; i < components.size(); ++i) {
    const std::string component = base::FilePath(components[i]).AsUTF8Unsafe();
    const std::string resource_id = storage_->GetChild(entry->resource_id(),
                                                       component);
    if (resource_id.empty())
      return scoped_ptr<DriveEntryProto>();

    entry = storage_->GetEntry(resource_id);
    DCHECK(entry);
    DCHECK_EQ(entry->base_name(), component);
  }
  return entry.Pass();
}

scoped_ptr<DriveResourceMetadata::GetEntryInfoWithFilePathResult>
DriveResourceMetadata::GetEntryInfoByResourceIdOnBlockingPool(
    const std::string& resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!resource_id.empty());

  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  if (entry) {
    error = DRIVE_FILE_OK;
    drive_file_path = GetFilePath(resource_id);
  } else {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  }

  return make_scoped_ptr(
      new GetEntryInfoWithFilePathResult(error, drive_file_path, entry.Pass()));
}

scoped_ptr<DriveResourceMetadata::GetEntryInfoResult>
DriveResourceMetadata::GetEntryInfoByPathOnBlockingPool(
    const base::FilePath& path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<DriveEntryProto> entry = FindEntryByPathSync(path);
  DriveFileError error = entry ? DRIVE_FILE_OK : DRIVE_FILE_ERROR_NOT_FOUND;

  return make_scoped_ptr(new GetEntryInfoResult(error, entry.Pass()));
}

scoped_ptr<DriveResourceMetadata::ReadDirectoryResult>
DriveResourceMetadata::ReadDirectoryByPathOnBlockingPool(
    const base::FilePath& path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<DriveEntryProtoVector> entries;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;

  scoped_ptr<DriveEntryProto> entry = FindEntryByPathSync(path);
  if (entry && entry->file_info().is_directory()) {
    entries = DirectoryChildrenToProtoVector(entry->resource_id());
    error = DRIVE_FILE_OK;
  } else if (entry && !entry->file_info().is_directory()) {
    error = DRIVE_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  }

  return make_scoped_ptr(new ReadDirectoryResult(error, entries.Pass()));
}

void DriveResourceMetadata::GetEntryInfoPairByPaths(
    const base::FilePath& first_path,
    const base::FilePath& second_path,
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

scoped_ptr<DriveResourceMetadata::GetEntryInfoWithFilePathResult>
DriveResourceMetadata::RefreshEntryOnBlockingPool(
    const DriveEntryProto& entry_proto) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<DriveEntryProto> entry =
      storage_->GetEntry(entry_proto.resource_id());
  if (!entry) {
    return make_scoped_ptr(
        new GetEntryInfoWithFilePathResult(DRIVE_FILE_ERROR_NOT_FOUND));
  }

  if (entry->parent_resource_id().empty() ||  // Rejct root.
      entry->file_info().is_directory() !=  // Reject incompatible input.
      entry_proto.file_info().is_directory()) {
    return make_scoped_ptr(
        new GetEntryInfoWithFilePathResult(DRIVE_FILE_ERROR_INVALID_OPERATION));
  }

  // Update data.
  scoped_ptr<DriveEntryProto> new_parent =
      GetDirectory(entry_proto.parent_resource_id());

  if (!new_parent) {
    return make_scoped_ptr(
        new GetEntryInfoWithFilePathResult(DRIVE_FILE_ERROR_NOT_FOUND));
  }

  // Remove from the old parent and add it to the new parent with the new data.
  DetachEntryFromDirectory(entry->resource_id());
  AddEntryToDirectory(CreateEntryWithProperBaseName(entry_proto));

  // Note that base_name is not the same for the new entry and entry_proto.
  scoped_ptr<DriveEntryProto> result_entry_proto =
      storage_->GetEntry(entry->resource_id());
  return make_scoped_ptr(
      new GetEntryInfoWithFilePathResult(DRIVE_FILE_OK,
                                         GetFilePath(entry->resource_id()),
                                         result_entry_proto.Pass()));
}

DriveResourceMetadata::FileMoveResult
DriveResourceMetadata::RefreshDirectoryOnBlockingPool(
    const DirectoryFetchInfo& directory_fetch_info,
    const DriveEntryProtoMap& entry_proto_map) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!directory_fetch_info.empty());

  scoped_ptr<DriveEntryProto> directory = storage_->GetEntry(
      directory_fetch_info.resource_id());

  if (!directory)
    return FileMoveResult(DRIVE_FILE_ERROR_NOT_FOUND);

  if (!directory->file_info().is_directory())
    return FileMoveResult(DRIVE_FILE_ERROR_NOT_A_DIRECTORY);

  directory->mutable_directory_specific_info()->set_changestamp(
      directory_fetch_info.changestamp());
  storage_->PutEntry(*directory);

  // First, go through the entry map. We'll handle existing entries and new
  // entries in the loop. We'll process deleted entries afterwards.
  for (DriveEntryProtoMap::const_iterator it = entry_proto_map.begin();
       it != entry_proto_map.end(); ++it) {
    const DriveEntryProto& entry_proto = it->second;
    // Skip if the parent resource ID does not match. This is needed to
    // handle entries with multiple parents. For such entries, the first
    // parent is picked and other parents are ignored, hence some entries may
    // have a parent resource ID which does not match the target directory's.
    //
    // TODO(satorux): Move the filtering logic to somewhere more appropriate.
    // crbug.com/193525.
    if (entry_proto.parent_resource_id() !=
        directory_fetch_info.resource_id()) {
      DVLOG(1) << "Wrong-parent entry rejected: " << entry_proto.resource_id();
      continue;
    }

    scoped_ptr<DriveEntryProto> existing_entry =
        storage_->GetEntry(entry_proto.resource_id());
    if (existing_entry)
      DetachEntryFromDirectory(entry_proto.resource_id());

    AddEntryToDirectory(CreateEntryWithProperBaseName(entry_proto));
  }

  // Go through the existing entries and remove deleted entries.
  scoped_ptr<DriveEntryProtoVector> entries =
      DirectoryChildrenToProtoVector(directory->resource_id());
  for (size_t i = 0; i < entries->size(); ++i) {
    const DriveEntryProto& entry_proto = entries->at(i);
    if (entry_proto_map.count(entry_proto.resource_id()) == 0)
      RemoveDirectoryChild(entry_proto.resource_id());
  }

  return FileMoveResult(DRIVE_FILE_OK, GetFilePath(directory->resource_id()));
}

DriveResourceMetadata::FileMoveResult
DriveResourceMetadata::AddEntryOnBlockingPool(
    const DriveEntryProto& entry_proto) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<DriveEntryProto> existing_entry =
      storage_->GetEntry(entry_proto.resource_id());
  if (existing_entry)
    return FileMoveResult(DRIVE_FILE_ERROR_EXISTS);

  scoped_ptr<DriveEntryProto> parent =
      GetDirectory(entry_proto.parent_resource_id());
  if (!parent)
    return FileMoveResult(DRIVE_FILE_ERROR_NOT_FOUND);

  AddEntryToDirectory(entry_proto);
  return FileMoveResult(DRIVE_FILE_OK, GetFilePath(entry_proto.resource_id()));
}

scoped_ptr<DriveEntryProto> DriveResourceMetadata::GetDirectory(
    const std::string& resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!resource_id.empty());

  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  return entry && entry->file_info().is_directory() ?
      entry.Pass() : scoped_ptr<DriveEntryProto>();
}

base::FilePath DriveResourceMetadata::GetFilePath(
    const std::string& resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  DCHECK(entry);
  base::FilePath path;
  if (!entry->parent_resource_id().empty())
    path = GetFilePath(entry->parent_resource_id());
  path = path.Append(base::FilePath::FromUTF8Unsafe(entry->base_name()));
  return path;
}

scoped_ptr<std::set<base::FilePath> >
DriveResourceMetadata::GetChildDirectoriesOnBlockingPool(
    const std::string& resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<std::set<base::FilePath> > changed_directories(
      new std::set<base::FilePath>);
  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  if (entry && entry->file_info().is_directory())
    GetDescendantDirectoryPaths(resource_id, changed_directories.get());

  return changed_directories.Pass();
}

void DriveResourceMetadata::GetDescendantDirectoryPaths(
    const std::string& directory_resource_id,
    std::set<base::FilePath>* child_directories) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  std::vector<std::string> children;
  storage_->GetChildren(directory_resource_id, &children);
  for (size_t i = 0; i < children.size(); ++i) {
    scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(children[i]);
    DCHECK(entry);
    if (entry->file_info().is_directory()) {
      child_directories->insert(GetFilePath(entry->resource_id()));
      GetDescendantDirectoryPaths(entry->resource_id(), child_directories);
    }
  }
}

void DriveResourceMetadata::RemoveAllOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  RemoveDirectoryChildren(util::kDriveGrandRootSpecialResourceId);
  SetUpDefaultEntries();
}

void DriveResourceMetadata::IterateEntriesOnBlockingPool(
    const IterateCallback& callback) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!callback.is_null());

  storage_->Iterate(callback);
}

void DriveResourceMetadata::GetEntryInfoPairByPathsAfterGetFirst(
    const base::FilePath& first_path,
    const base::FilePath& second_path,
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
    const base::FilePath& second_path,
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

void DriveResourceMetadata::AddEntryToDirectory(const DriveEntryProto& entry) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  DriveEntryProto updated_entry(entry);

  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  SetBaseNameFromTitle(&updated_entry);

  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int modifier = 1;
  std::string new_base_name = updated_entry.base_name();
  while (!storage_->GetChild(entry.parent_resource_id(),
                             new_base_name).empty()) {
    base::FilePath new_path =
        base::FilePath::FromUTF8Unsafe(updated_entry.base_name());
    new_path =
        new_path.InsertBeforeExtension(base::StringPrintf(" (%d)", ++modifier));
    new_base_name = new_path.AsUTF8Unsafe();
  }
  updated_entry.set_base_name(new_base_name);

  // Setup child and parent links.
  storage_->PutChild(entry.parent_resource_id(),
                     updated_entry.base_name(),
                     updated_entry.resource_id());

  // Add the entry to resource map.
  storage_->PutEntry(updated_entry);
}

void DriveResourceMetadata::RemoveDirectoryChild(
    const std::string& child_resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(child_resource_id);
  DCHECK(entry);
  DetachEntryFromDirectory(child_resource_id);
  storage_->RemoveEntry(entry->resource_id());
  if (entry->file_info().is_directory())
    RemoveDirectoryChildren(child_resource_id);
}

void DriveResourceMetadata::DetachEntryFromDirectory(
    const std::string& child_resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(child_resource_id);
  DCHECK(entry);

  // entry must be present in this directory.
  DCHECK_EQ(entry->resource_id(),
            storage_->GetChild(entry->parent_resource_id(),
                               entry->base_name()));

  storage_->RemoveChild(entry->parent_resource_id(), entry->base_name());
}

void DriveResourceMetadata::RemoveDirectoryChildren(
    const std::string& directory_resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  std::vector<std::string> children;
  storage_->GetChildren(directory_resource_id, &children);
  for (size_t i = 0; i < children.size(); ++i)
    RemoveDirectoryChild(children[i]);
}

scoped_ptr<DriveEntryProtoVector>
DriveResourceMetadata::DirectoryChildrenToProtoVector(
    const std::string& directory_resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<DriveEntryProtoVector> entries(new DriveEntryProtoVector);
  std::vector<std::string> children;
  storage_->GetChildren(directory_resource_id, &children);
  for (size_t i = 0; i < children.size(); ++i) {
    scoped_ptr<DriveEntryProto> child = storage_->GetEntry(children[i]);
    DCHECK(child);
    entries->push_back(*child);
  }
  return entries.Pass();
}

}  // namespace drive
