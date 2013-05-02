// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_metadata.h"

#include "base/file_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace {

// Sets entry's base name from its title and other attributes.
void SetBaseNameFromTitle(ResourceEntry* entry) {
  std::string base_name = entry->title();
  if (entry->has_file_specific_info() &&
      entry->file_specific_info().is_hosted_document()) {
    base_name += entry->file_specific_info().document_extension();
  }
  entry->set_base_name(util::EscapeUtf8FileName(base_name));
}

// Creates an entry by copying |source|, and setting the base name properly.
ResourceEntry CreateEntryWithProperBaseName(const ResourceEntry& source) {
  ResourceEntry entry(source);
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

// Returns true if enough disk space is avilable for DB operation.
// TODO(hashimoto): Merge this with FileCache's FreeDiskSpaceGetterInterface.
bool EnoughDiskSpaceIsAvailableForDBOperation(const base::FilePath& path) {
  const int64 kRequiredDiskSpaceInMB = 128;  // 128 MB seems to be large enough.
  return base::SysInfo::AmountOfFreeDiskSpace(path) >=
      kRequiredDiskSpaceInMB * (1 << 20);
}

// Runs |callback| with arguments.
void RunGetEntryInfoCallback(const GetEntryInfoCallback& callback,
                             scoped_ptr<ResourceEntry> entry,
                             FileError error) {
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK)
    entry.reset();
  callback.Run(error, entry.Pass());
}

// Runs |callback| with arguments.
void RunGetEntryInfoWithFilePathCallback(
    const GetEntryInfoWithFilePathCallback& callback,
    base::FilePath* path,
    scoped_ptr<ResourceEntry> entry,
    FileError error) {
  DCHECK(!callback.is_null());
  DCHECK(path);

  if (error != FILE_ERROR_OK)
    entry.reset();
  callback.Run(error, *path, entry.Pass());
}

// Runs |callback| with arguments.
void RunReadDirectoryCallback(const ReadDirectoryCallback& callback,
                              scoped_ptr<ResourceEntryVector> entries,
                              FileError error) {
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK)
    entries.reset();
  callback.Run(error, entries.Pass());
}

// Runs |callback| with arguments.
void RunFileMoveCallback(const FileMoveCallback& callback,
                         base::FilePath* path,
                         FileError error) {
  DCHECK(!callback.is_null());
  DCHECK(path);

  callback.Run(error, *path);
}

// Helper function to run tasks with FileMoveCallback.
void PostFileMoveTask(
    base::TaskRunner* task_runner,
    const base::Callback<FileError(base::FilePath* out_file_path)>& task,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!task.is_null());
  DCHECK(!callback.is_null());

  base::FilePath* file_path = new base::FilePath;

  base::PostTaskAndReplyWithResult(
      task_runner,
      FROM_HERE,
      base::Bind(task, file_path),
      base::Bind(&RunFileMoveCallback, callback, base::Owned(file_path)));
}

}  // namespace

std::string DirectoryFetchInfo::ToString() const {
  return ("resource_id: " + resource_id_ +
          ", changestamp: " + base::Int64ToString(changestamp_));
}

EntryInfoResult::EntryInfoResult() : error(FILE_ERROR_FAILED) {
}

EntryInfoResult::~EntryInfoResult() {
}

EntryInfoPairResult::EntryInfoPairResult() {
}

EntryInfoPairResult::~EntryInfoPairResult() {
}

namespace internal {

ResourceMetadata::ResourceMetadata(
    const base::FilePath& data_directory_path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : data_directory_path_(data_directory_path),
      blocking_task_runner_(blocking_task_runner),
      storage_(new ResourceMetadataStorage(data_directory_path)),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ResourceMetadata::Initialize(const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::InitializeOnBlockingPool,
                 base::Unretained(this)),
      callback);
}

void ResourceMetadata::Destroy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  weak_ptr_factory_.InvalidateWeakPtrs();
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ResourceMetadata::DestroyOnBlockingPool,
                 base::Unretained(this)));
}

void ResourceMetadata::Reset(const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::ResetOnBlockingPool,
                 base::Unretained(this)),
      callback);
}

ResourceMetadata::~ResourceMetadata() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
}

FileError ResourceMetadata::InitializeOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
    return FILE_ERROR_NO_SPACE;

  // Initialize the storage.
  if (!storage_->Initialize())
    return FILE_ERROR_FAILED;

  if (!SetUpDefaultEntries())
    return FILE_ERROR_FAILED;

  return FILE_ERROR_OK;
}

bool ResourceMetadata::SetUpDefaultEntries() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  // Initialize the grand root and "other" entries. "/drive" and "/drive/other".
  // As an intermediate change, "/drive/root" is also added here.
  if (!storage_->GetEntry(util::kDriveGrandRootSpecialResourceId)) {
    ResourceEntry root;
    root.mutable_file_info()->set_is_directory(true);
    root.set_resource_id(util::kDriveGrandRootSpecialResourceId);
    root.set_title(util::kDriveGrandRootDirName);
    if (!storage_->PutEntry(CreateEntryWithProperBaseName(root)))
      return false;
  }
  if (!storage_->GetEntry(util::kDriveOtherDirSpecialResourceId)) {
    if (!PutEntryUnderDirectory(util::CreateOtherDirEntry()))
      return false;
  }
  return true;
}

void ResourceMetadata::DestroyOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  delete this;
}

FileError ResourceMetadata::ResetOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
    return FILE_ERROR_NO_SPACE;

  if (!storage_->SetLargestChangestamp(0) ||
      !RemoveEntryRecursively(util::kDriveGrandRootSpecialResourceId) ||
      !SetUpDefaultEntries())
    return FILE_ERROR_FAILED;

  return FILE_ERROR_OK;
}

void ResourceMetadata::GetLargestChangestamp(
    const GetChangestampCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetLargestChangestampOnBlockingPool,
                 base::Unretained(this)),
      callback);
}

void ResourceMetadata::SetLargestChangestamp(
    int64 value,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::SetLargestChangestampOnBlockingPool,
                 base::Unretained(this),
                 value),
      callback);
}

void ResourceMetadata::AddEntry(const ResourceEntry& entry,
                                const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  PostFileMoveTask(blocking_task_runner_,
                   base::Bind(&ResourceMetadata::AddEntryOnBlockingPool,
                              base::Unretained(this),
                              entry),
                   callback);
}

void ResourceMetadata::MoveEntryToDirectory(
    const base::FilePath& file_path,
    const base::FilePath& directory_path,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  PostFileMoveTask(
      blocking_task_runner_,
      base::Bind(&ResourceMetadata::MoveEntryToDirectoryOnBlockingPool,
                 base::Unretained(this),
                 file_path,
                 directory_path),
      callback);
}

void ResourceMetadata::RenameEntry(const base::FilePath& file_path,
                                   const std::string& new_name,
                                   const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  PostFileMoveTask(blocking_task_runner_,
                   base::Bind(&ResourceMetadata::RenameEntryOnBlockingPool,
                              base::Unretained(this),
                              file_path,
                              new_name),
                   callback);
}

void ResourceMetadata::RemoveEntry(const std::string& resource_id,
                                   const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  PostFileMoveTask(blocking_task_runner_,
                   base::Bind(&ResourceMetadata::RemoveEntryOnBlockingPool,
                              base::Unretained(this),
                              resource_id),
                   callback);
}

void ResourceMetadata::GetEntryInfoByResourceId(
    const std::string& resource_id,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::FilePath* file_path = new base::FilePath;
  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  ResourceEntry* entry_ptr = entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetEntryInfoByResourceIdOnBlockingPool,
                 base::Unretained(this),
                 resource_id,
                 file_path,
                 entry_ptr),
      base::Bind(&RunGetEntryInfoWithFilePathCallback,
                 callback,
                 base::Owned(file_path),
                 base::Passed(&entry)));
}

void ResourceMetadata::GetEntryInfoByPath(
    const base::FilePath& file_path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  ResourceEntry* entry_ptr = entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetEntryInfoByPathOnBlockingPool,
                 base::Unretained(this),
                 file_path,
                 entry_ptr),
      base::Bind(&RunGetEntryInfoCallback,
                 callback,
                 base::Passed(&entry)));
}

void ResourceMetadata::ReadDirectoryByPath(
    const base::FilePath& file_path,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<ResourceEntryVector> entries(new ResourceEntryVector);
  ResourceEntryVector* entries_ptr = entries.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::ReadDirectoryByPathOnBlockingPool,
                 base::Unretained(this),
                 file_path,
                 entries_ptr),
      base::Bind(&RunReadDirectoryCallback,
                 callback,
                 base::Passed(&entries)));
}

void ResourceMetadata::RefreshEntry(
    const ResourceEntry& in_entry,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::FilePath* file_path = new base::FilePath;
  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  ResourceEntry* entry_ptr = entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::RefreshEntryOnBlockingPool,
                 base::Unretained(this),
                 in_entry,
                 file_path,
                 entry_ptr),
      base::Bind(&RunGetEntryInfoWithFilePathCallback,
                 callback,
                 base::Owned(file_path),
                 base::Passed(&entry)));
}

void ResourceMetadata::RefreshDirectory(
    const DirectoryFetchInfo& directory_fetch_info,
    const ResourceEntryMap& entry_map,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  PostFileMoveTask(
      blocking_task_runner_,
      base::Bind(&ResourceMetadata::RefreshDirectoryOnBlockingPool,
                 base::Unretained(this),
                 directory_fetch_info,
                 entry_map),
      callback);
}

void ResourceMetadata::GetChildDirectories(
    const std::string& resource_id,
    const GetChildDirectoriesCallback& changed_dirs_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!changed_dirs_callback.is_null());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetChildDirectoriesOnBlockingPool,
                 base::Unretained(this),
                 resource_id),
      base::Bind(&RunGetChildDirectoriesCallbackWithResult,
                 changed_dirs_callback));
}

void ResourceMetadata::IterateEntries(
    const IterateCallback& iterate_callback,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!iterate_callback.is_null());
  DCHECK(!completion_callback.is_null());

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ResourceMetadata::IterateEntriesOnBlockingPool,
                 base::Unretained(this),
                 iterate_callback),
      completion_callback);
}

int64 ResourceMetadata::GetLargestChangestampOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  return storage_->GetLargestChangestamp();
}

FileError ResourceMetadata::SetLargestChangestampOnBlockingPool(
    int64 value) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
    return FILE_ERROR_NO_SPACE;

  storage_->SetLargestChangestamp(value);
  return FILE_ERROR_OK;
}

FileError ResourceMetadata::MoveEntryToDirectoryOnBlockingPool(
    const base::FilePath& file_path,
    const base::FilePath& directory_path,
    base::FilePath* out_file_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!directory_path.empty());
  DCHECK(!file_path.empty());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
    return FILE_ERROR_NO_SPACE;

  scoped_ptr<ResourceEntry> entry = FindEntryByPathSync(file_path);
  scoped_ptr<ResourceEntry> destination = FindEntryByPathSync(directory_path);
  if (!entry || !destination)
    return FILE_ERROR_NOT_FOUND;
  if (!destination->file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  entry->set_parent_resource_id(destination->resource_id());

  return RefreshEntryOnBlockingPool(*entry, out_file_path, NULL);
}

FileError ResourceMetadata::RenameEntryOnBlockingPool(
    const base::FilePath& file_path,
    const std::string& new_name,
    base::FilePath* out_file_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!file_path.empty());
  DCHECK(!new_name.empty());

  DVLOG(1) << "RenameEntry " << file_path.value() << " to " << new_name;

  if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
    return FILE_ERROR_NO_SPACE;

  scoped_ptr<ResourceEntry> entry = FindEntryByPathSync(file_path);
  if (!entry)
    return FILE_ERROR_NOT_FOUND;

  if (base::FilePath::FromUTF8Unsafe(new_name) == file_path.BaseName())
    return FILE_ERROR_EXISTS;

  entry->set_title(new_name);
  return RefreshEntryOnBlockingPool(*entry, out_file_path, NULL);
}

FileError ResourceMetadata::RemoveEntryOnBlockingPool(
    const std::string& resource_id,
    base::FilePath* out_file_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
    return FILE_ERROR_NO_SPACE;

  // Disallow deletion of special entries "/drive" and "/drive/other".
  if (util::IsSpecialResourceId(resource_id))
    return FILE_ERROR_ACCESS_DENIED;

  scoped_ptr<ResourceEntry> entry = storage_->GetEntry(resource_id);
  if (!entry)
    return FILE_ERROR_NOT_FOUND;

  if (!RemoveEntryRecursively(entry->resource_id()))
    return FILE_ERROR_FAILED;

  if (out_file_path)
    *out_file_path = GetFilePath(entry->parent_resource_id());
  return FILE_ERROR_OK;
}

scoped_ptr<ResourceEntry> ResourceMetadata::FindEntryByPathSync(
    const base::FilePath& file_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  // Start from the root.
  scoped_ptr<ResourceEntry> entry =
      storage_->GetEntry(util::kDriveGrandRootSpecialResourceId);
  DCHECK(entry);
  DCHECK(entry->parent_resource_id().empty());

  // Check the first component.
  std::vector<base::FilePath::StringType> components;
  file_path.GetComponents(&components);
  if (components.empty() ||
      base::FilePath(components[0]).AsUTF8Unsafe() != entry->base_name())
    return scoped_ptr<ResourceEntry>();

  // Iterate over the remaining components.
  for (size_t i = 1; i < components.size(); ++i) {
    const std::string component = base::FilePath(components[i]).AsUTF8Unsafe();
    const std::string resource_id = storage_->GetChild(entry->resource_id(),
                                                       component);
    if (resource_id.empty())
      return scoped_ptr<ResourceEntry>();

    entry = storage_->GetEntry(resource_id);
    DCHECK(entry);
    DCHECK_EQ(entry->base_name(), component);
  }
  return entry.Pass();
}

FileError ResourceMetadata::GetEntryInfoByResourceIdOnBlockingPool(
    const std::string& resource_id,
    base::FilePath* out_file_path,
    ResourceEntry* out_entry) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!resource_id.empty());

  scoped_ptr<ResourceEntry> entry = storage_->GetEntry(resource_id);
  if (!entry)
    return FILE_ERROR_NOT_FOUND;

  if (out_file_path)
    *out_file_path = GetFilePath(resource_id);
  if (out_entry)
    *out_entry = *entry;

  return FILE_ERROR_OK;
}

FileError ResourceMetadata::GetEntryInfoByPathOnBlockingPool(
    const base::FilePath& path,
    ResourceEntry* out_entry) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(out_entry);

  scoped_ptr<ResourceEntry> entry = FindEntryByPathSync(path);
  if (!entry)
    return FILE_ERROR_NOT_FOUND;

  *out_entry = *entry;
  return FILE_ERROR_OK;
}

FileError ResourceMetadata::ReadDirectoryByPathOnBlockingPool(
    const base::FilePath& path,
    ResourceEntryVector* out_entries) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(out_entries);

  scoped_ptr<ResourceEntry> entry = FindEntryByPathSync(path);
  if (!entry)
    return FILE_ERROR_NOT_FOUND;

  if (!entry->file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  DirectoryChildrenToProtoVector(entry->resource_id())->swap(*out_entries);
  return FILE_ERROR_OK;
}

void ResourceMetadata::GetEntryInfoPairByPaths(
    const base::FilePath& first_path,
    const base::FilePath& second_path,
    const GetEntryInfoPairCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Get the first entry.
  GetEntryInfoByPath(
      first_path,
      base::Bind(&ResourceMetadata::GetEntryInfoPairByPathsAfterGetFirst,
                 weak_ptr_factory_.GetWeakPtr(),
                 first_path,
                 second_path,
                 callback));
}

FileError ResourceMetadata::RefreshEntryOnBlockingPool(
    const ResourceEntry& entry,
    base::FilePath* out_file_path,
    ResourceEntry* out_entry) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
    return FILE_ERROR_NO_SPACE;

  scoped_ptr<ResourceEntry> old_entry =
      storage_->GetEntry(entry.resource_id());
  if (!old_entry)
    return FILE_ERROR_NOT_FOUND;

  if (old_entry->parent_resource_id().empty() ||  // Rejct root.
      old_entry->file_info().is_directory() !=  // Reject incompatible input.
      entry.file_info().is_directory())
    return FILE_ERROR_INVALID_OPERATION;

  // Update data.
  scoped_ptr<ResourceEntry> new_parent =
      GetDirectory(entry.parent_resource_id());

  if (!new_parent)
    return FILE_ERROR_NOT_FOUND;

  // Remove from the old parent and add it to the new parent with the new data.
  if (!PutEntryUnderDirectory(CreateEntryWithProperBaseName(entry)))
    return FILE_ERROR_FAILED;

  if (out_file_path)
    *out_file_path = GetFilePath(entry.resource_id());

  if (out_entry) {
    // Note that base_name is not the same for the new entry and entry.
    scoped_ptr<ResourceEntry> result_entry =
        storage_->GetEntry(entry.resource_id());
    DCHECK(result_entry);
    *out_entry = *result_entry;
  }
  return FILE_ERROR_OK;
}

FileError ResourceMetadata::RefreshDirectoryOnBlockingPool(
    const DirectoryFetchInfo& directory_fetch_info,
    const ResourceEntryMap& entry_map,
    base::FilePath* out_file_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!directory_fetch_info.empty());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
    return FILE_ERROR_NO_SPACE;

  scoped_ptr<ResourceEntry> directory = storage_->GetEntry(
      directory_fetch_info.resource_id());

  if (!directory)
    return FILE_ERROR_NOT_FOUND;

  if (!directory->file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  directory->mutable_directory_specific_info()->set_changestamp(
      directory_fetch_info.changestamp());
  storage_->PutEntry(*directory);

  // First, go through the entry map. We'll handle existing entries and new
  // entries in the loop. We'll process deleted entries afterwards.
  for (ResourceEntryMap::const_iterator it = entry_map.begin();
       it != entry_map.end(); ++it) {
    if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
      return FILE_ERROR_NO_SPACE;

    const ResourceEntry& entry = it->second;
    // Skip if the parent resource ID does not match. This is needed to
    // handle entries with multiple parents. For such entries, the first
    // parent is picked and other parents are ignored, hence some entries may
    // have a parent resource ID which does not match the target directory's.
    //
    // TODO(satorux): Move the filtering logic to somewhere more appropriate.
    // crbug.com/193525.
    if (entry.parent_resource_id() !=
        directory_fetch_info.resource_id()) {
      DVLOG(1) << "Wrong-parent entry rejected: " << entry.resource_id();
      continue;
    }

    if (!PutEntryUnderDirectory(CreateEntryWithProperBaseName(entry)))
      return FILE_ERROR_FAILED;
  }

  // Go through the existing entries and remove deleted entries.
  scoped_ptr<ResourceEntryVector> entries =
      DirectoryChildrenToProtoVector(directory->resource_id());
  for (size_t i = 0; i < entries->size(); ++i) {
    if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
      return FILE_ERROR_NO_SPACE;

    const ResourceEntry& entry = entries->at(i);
    if (entry_map.count(entry.resource_id()) == 0) {
      if (!RemoveEntryRecursively(entry.resource_id()))
        return FILE_ERROR_FAILED;
    }
  }

  if (out_file_path)
    *out_file_path = GetFilePath(directory->resource_id());

  return FILE_ERROR_OK;
}

FileError ResourceMetadata::AddEntryOnBlockingPool(
    const ResourceEntry& entry,
    base::FilePath* out_file_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(data_directory_path_))
    return FILE_ERROR_NO_SPACE;

  scoped_ptr<ResourceEntry> existing_entry =
      storage_->GetEntry(entry.resource_id());
  if (existing_entry)
    return FILE_ERROR_EXISTS;

  scoped_ptr<ResourceEntry> parent =
      GetDirectory(entry.parent_resource_id());
  if (!parent)
    return FILE_ERROR_NOT_FOUND;

  if (!PutEntryUnderDirectory(entry))
    return FILE_ERROR_FAILED;

  if (out_file_path)
    *out_file_path = GetFilePath(entry.resource_id());

  return FILE_ERROR_OK;
}

scoped_ptr<ResourceEntry> ResourceMetadata::GetDirectory(
    const std::string& resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!resource_id.empty());

  scoped_ptr<ResourceEntry> entry = storage_->GetEntry(resource_id);
  return entry && entry->file_info().is_directory() ?
      entry.Pass() : scoped_ptr<ResourceEntry>();
}

base::FilePath ResourceMetadata::GetFilePath(
    const std::string& resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<ResourceEntry> entry = storage_->GetEntry(resource_id);
  DCHECK(entry);
  base::FilePath path;
  if (!entry->parent_resource_id().empty())
    path = GetFilePath(entry->parent_resource_id());
  path = path.Append(base::FilePath::FromUTF8Unsafe(entry->base_name()));
  return path;
}

scoped_ptr<std::set<base::FilePath> >
ResourceMetadata::GetChildDirectoriesOnBlockingPool(
    const std::string& resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<std::set<base::FilePath> > changed_directories(
      new std::set<base::FilePath>);
  scoped_ptr<ResourceEntry> entry = storage_->GetEntry(resource_id);
  if (entry && entry->file_info().is_directory())
    GetDescendantDirectoryPaths(resource_id, changed_directories.get());

  return changed_directories.Pass();
}

void ResourceMetadata::GetDescendantDirectoryPaths(
    const std::string& directory_resource_id,
    std::set<base::FilePath>* child_directories) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  std::vector<std::string> children;
  storage_->GetChildren(directory_resource_id, &children);
  for (size_t i = 0; i < children.size(); ++i) {
    scoped_ptr<ResourceEntry> entry = storage_->GetEntry(children[i]);
    DCHECK(entry);
    if (entry->file_info().is_directory()) {
      child_directories->insert(GetFilePath(entry->resource_id()));
      GetDescendantDirectoryPaths(entry->resource_id(), child_directories);
    }
  }
}

void ResourceMetadata::IterateEntriesOnBlockingPool(
    const IterateCallback& callback) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!callback.is_null());

  storage_->Iterate(callback);
}

void ResourceMetadata::GetEntryInfoPairByPathsAfterGetFirst(
    const base::FilePath& first_path,
    const base::FilePath& second_path,
    const GetEntryInfoPairCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<EntryInfoPairResult> result(new EntryInfoPairResult);
  result->first.path = first_path;
  result->first.error = error;
  result->first.entry = entry.Pass();

  // If the first one is not found, don't continue.
  if (error != FILE_ERROR_OK) {
    callback.Run(result.Pass());
    return;
  }

  // Get the second entry.
  GetEntryInfoByPath(
      second_path,
      base::Bind(&ResourceMetadata::GetEntryInfoPairByPathsAfterGetSecond,
                 weak_ptr_factory_.GetWeakPtr(),
                 second_path,
                 callback,
                 base::Passed(&result)));
}

void ResourceMetadata::GetEntryInfoPairByPathsAfterGetSecond(
    const base::FilePath& second_path,
    const GetEntryInfoPairCallback& callback,
    scoped_ptr<EntryInfoPairResult> result,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  result->second.path = second_path;
  result->second.error = error;
  result->second.entry = entry.Pass();

  callback.Run(result.Pass());
}

bool ResourceMetadata::PutEntryUnderDirectory(
    const ResourceEntry& entry) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  ResourceEntry updated_entry(entry);

  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  SetBaseNameFromTitle(&updated_entry);

  // Do file name de-duplication - Keep changing |entry|'s name until there is
  // no other entry with the same name under the parent.
  int modifier = 1;
  std::string new_base_name = updated_entry.base_name();
  while (true) {
    const std::string existing_entry_id =
        storage_->GetChild(entry.parent_resource_id(), new_base_name);
    if (existing_entry_id.empty() || existing_entry_id == entry.resource_id())
      break;

    base::FilePath new_path =
        base::FilePath::FromUTF8Unsafe(updated_entry.base_name());
    new_path =
        new_path.InsertBeforeExtension(base::StringPrintf(" (%d)", ++modifier));
    new_base_name = new_path.AsUTF8Unsafe();
  }
  updated_entry.set_base_name(new_base_name);

  // Add the entry to resource map.
  return storage_->PutEntry(updated_entry);
}

bool ResourceMetadata::RemoveEntryRecursively(
    const std::string& resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<ResourceEntry> entry = storage_->GetEntry(resource_id);
  DCHECK(entry);

  if (entry->file_info().is_directory()) {
    std::vector<std::string> children;
    storage_->GetChildren(resource_id, &children);
    for (size_t i = 0; i < children.size(); ++i) {
      if (!RemoveEntryRecursively(children[i]))
        return false;
    }
  }
  return storage_->RemoveEntry(resource_id);
}

scoped_ptr<ResourceEntryVector>
ResourceMetadata::DirectoryChildrenToProtoVector(
    const std::string& directory_resource_id) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<ResourceEntryVector> entries(new ResourceEntryVector);
  std::vector<std::string> children;
  storage_->GetChildren(directory_resource_id, &children);
  for (size_t i = 0; i < children.size(); ++i) {
    scoped_ptr<ResourceEntry> child = storage_->GetEntry(children[i]);
    DCHECK(child);
    entries->push_back(*child);
  }
  return entries.Pass();
}

}  // namespace internal
}  // namespace drive
