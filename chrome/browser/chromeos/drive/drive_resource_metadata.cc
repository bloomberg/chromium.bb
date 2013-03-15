// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"

#include <leveldb/db.h>
#include <stack>
#include <utility>

#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata_storage.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace {

// Posts |error| to |callback| asynchronously. |callback| must not be null.
void PostFileMoveCallbackError(const FileMoveCallback& callback,
                               DriveFileError error) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::FilePath()));
}

// Posts |error| to |callback| asynchronously. |callback| must not be null.
void PostGetEntryInfoWithFilePathCallbackError(
    const GetEntryInfoWithFilePathCallback& callback,
    DriveFileError error) {
  scoped_ptr<DriveEntryProto> proto;
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::FilePath(), base::Passed(&proto)));
}

// Adds per-directory changestamps to |root| and all sub directories.
void AddPerDirectoryChangestamps(DriveDirectoryProto* root, int64 changestamp) {
  std::stack<DriveDirectoryProto*> stack;
  stack.push(root);
  while (!stack.empty()) {
    DriveDirectoryProto* directory = stack.top();
    stack.pop();
    directory->mutable_drive_entry()->mutable_directory_specific_info()
        ->set_changestamp(changestamp);
    for (int i = 0; i < directory->child_directories_size(); ++i)
      stack.push(directory->mutable_child_directories(i));
  }
}

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

}  // namespace

EntryInfoResult::EntryInfoResult() : error(DRIVE_FILE_ERROR_FAILED) {
}

EntryInfoResult::~EntryInfoResult() {
}

EntryInfoPairResult::EntryInfoPairResult() {
}

EntryInfoPairResult::~EntryInfoPairResult() {
}

// DriveResourceMetadata class implementation.

DriveResourceMetadata::DriveResourceMetadata(
    const std::string& root_resource_id)
    : blocking_task_runner_(NULL),
      storage_(new DriveResourceMetadataStorageMemory),
      root_resource_id_(root_resource_id),
      serialized_size_(0),
      largest_changestamp_(0),
      loaded_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DriveEntryProto root;
  root.mutable_file_info()->set_is_directory(true);
  root.set_resource_id(root_resource_id);
  root.set_title(kDriveRootDirectory);
  storage_->PutEntry(CreateEntryWithProperBaseName(root));
}

DriveResourceMetadata::~DriveResourceMetadata() {
  ClearRoot();
}

void DriveResourceMetadata::ClearRoot() {
  if (root_resource_id_.empty())
    return;

  RemoveDirectoryChildren(root_resource_id_);
  storage_->RemoveEntry(root_resource_id_);
  root_resource_id_.clear();
}

void DriveResourceMetadata::GetLargestChangestamp(
    const GetChangestampCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, largest_changestamp_));
}

void DriveResourceMetadata::SetLargestChangestamp(
    int64 value,
    const FileOperationCallback& callback) {
  largest_changestamp_ = value;
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, DRIVE_FILE_OK));
}

void DriveResourceMetadata::MoveEntryToDirectory(
    const base::FilePath& file_path,
    const base::FilePath& directory_path,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_path.empty());
  DCHECK(!file_path.empty());
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> entry = FindEntryByPathSync(file_path);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  // Cannot move an entry without its parent. (i.e. the root)
  if (entry->parent_resource_id().empty()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_INVALID_OPERATION);
    return;
  }

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
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, error, moved_file_path));
}

void DriveResourceMetadata::RenameEntry(
  const base::FilePath& file_path,
  const base::FilePath::StringType& new_name,
  const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!file_path.empty());
  DCHECK(!new_name.empty());
  DCHECK(!callback.is_null());

  DVLOG(1) << "RenameEntry " << file_path.value() << " to " << new_name;
  scoped_ptr<DriveEntryProto> entry = FindEntryByPathSync(file_path);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  if (new_name == file_path.BaseName().value()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_EXISTS);
    return;
  }

  entry->set_title(new_name);
  storage_->PutEntry(*entry);

  // After changing the title of the entry, call MoveEntryToDirectory to
  // remove the entry from its parent directory and then add it back in order to
  // go through the file name de-duplication.
  // TODO(achuith/satorux/zel): This code is fragile. The title has been
  // changed, but not the file_name. MoveEntryToDirectory calls RemoveChild to
  // remove the child based on the old file_name, and then re-adds the child by
  // first assigning the new title to file_name. http://crbug.com/30157
  MoveEntryToDirectory(file_path,
                       GetFilePath(entry->parent_resource_id()), callback);
}

void DriveResourceMetadata::RemoveEntry(const std::string& resource_id,
                                        const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Disallow deletion of root.
  if (resource_id == root_resource_id_) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_ACCESS_DENIED);
    return;
  }

  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  RemoveDirectoryChild(entry->resource_id());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK,
                 GetFilePath(entry->parent_resource_id())));
}

scoped_ptr<DriveEntryProto> DriveResourceMetadata::FindEntryByPathSync(
    const base::FilePath& file_path) {
  scoped_ptr<DriveEntryProto> current_dir =
      storage_->GetEntry(root_resource_id_);
  DCHECK(current_dir);

  if (file_path == GetFilePath(current_dir->resource_id()))
      return current_dir.Pass();

  std::vector<base::FilePath::StringType> components;
  file_path.GetComponents(&components);

  for (size_t i = 1; i < components.size() && current_dir; ++i) {
    std::string resource_id =
        storage_->GetChild(current_dir->resource_id(), components[i]);
    if (resource_id.empty())
      break;

    scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
    DCHECK(entry);

    if (i == components.size() - 1)  // Last component.
      return entry.Pass();
    if (!entry->file_info().is_directory())
      break;
    current_dir = entry.Pass();
  }
  return scoped_ptr<DriveEntryProto>();
}

void DriveResourceMetadata::GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  if (entry) {
    error = DRIVE_FILE_OK;
    drive_file_path = GetFilePath(resource_id);
  } else {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, drive_file_path, base::Passed(&entry)));
}

void DriveResourceMetadata::GetEntryInfoByPath(
    const base::FilePath& path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> entry = FindEntryByPathSync(path);
  DriveFileError error = entry ? DRIVE_FILE_OK : DRIVE_FILE_ERROR_NOT_FOUND;

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&entry)));
}

void DriveResourceMetadata::ReadDirectoryByPath(
    const base::FilePath& path,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

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

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&entries)));
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

void DriveResourceMetadata::RefreshEntry(
    const DriveEntryProto& entry_proto,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> entry =
      storage_->GetEntry(entry_proto.resource_id());
  if (!entry) {
    PostGetEntryInfoWithFilePathCallbackError(
        callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  // Reject incompatible input.
  if (entry->file_info().is_directory() !=
      entry_proto.file_info().is_directory()) {
    PostGetEntryInfoWithFilePathCallbackError(
        callback, DRIVE_FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // Update data.
  if (entry->resource_id() != root_resource_id_) {
    scoped_ptr<DriveEntryProto> new_parent =
        GetDirectory(entry_proto.parent_resource_id());

    if (!new_parent) {
      PostGetEntryInfoWithFilePathCallbackError(
          callback, DRIVE_FILE_ERROR_NOT_FOUND);
      return;
    }

    // Remove from the old parent, update the entry, and add it to the new
    // parent.
    DetachEntryFromDirectory(entry->resource_id());
    AddEntryToDirectory(CreateEntryWithProperBaseName(entry_proto));
  } else {
    // root has no parent.
    storage_->PutEntry(CreateEntryWithProperBaseName(entry_proto));
  }

  // Note that base_name is not the same for the new entry and entry_proto.
  scoped_ptr<DriveEntryProto> result_entry_proto =
      storage_->GetEntry(entry->resource_id());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 DRIVE_FILE_OK,
                 GetFilePath(entry->resource_id()),
                 base::Passed(&result_entry_proto)));
}

void DriveResourceMetadata::RefreshDirectory(
    const DirectoryFetchInfo& directory_fetch_info,
    const DriveEntryProtoMap& entry_proto_map,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!directory_fetch_info.empty());

  scoped_ptr<DriveEntryProto> directory = storage_->GetEntry(
      directory_fetch_info.resource_id());

  if (!directory) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  if (!directory->file_info().is_directory()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

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

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK,
                 GetFilePath(directory->resource_id())));
}

void DriveResourceMetadata::AddEntry(const DriveEntryProto& entry_proto,
                                     const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> parent =
      GetDirectory(entry_proto.parent_resource_id());
  if (!parent) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  AddEntryToDirectory(entry_proto);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK,
                 GetFilePath(entry_proto.resource_id())));
}

scoped_ptr<DriveEntryProto> DriveResourceMetadata::GetDirectory(
    const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());

  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  return entry && entry->file_info().is_directory() ?
      entry.Pass() : scoped_ptr<DriveEntryProto>();
}

base::FilePath DriveResourceMetadata::GetFilePath(
    const std::string& resource_id) {
  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  DCHECK(entry);
  base::FilePath path;
  if (!entry->parent_resource_id().empty())
    path = GetFilePath(entry->parent_resource_id());
  path = path.Append(entry->base_name());
  return path;
}

void DriveResourceMetadata::GetChildDirectories(
    const std::string& resource_id,
    const GetChildDirectoriesCallback& changed_dirs_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!changed_dirs_callback.is_null());

  std::set<base::FilePath> changed_directories;
  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(resource_id);
  if (entry && entry->file_info().is_directory())
    GetDescendantDirectoryPaths(resource_id, &changed_directories);

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(changed_dirs_callback, changed_directories));
}

void DriveResourceMetadata::GetDescendantDirectoryPaths(
    const std::string& directory_resource_id,
    std::set<base::FilePath>* child_directories) {
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

void DriveResourceMetadata::RemoveAll(const base::Closure& callback) {
  RemoveDirectoryChildren(root_resource_id_);
  base::MessageLoopProxy::current()->PostTask(FROM_HERE, callback);
}

void DriveResourceMetadata::SerializeToString(std::string* serialized_proto) {
  DriveRootDirectoryProto proto;
  DirectoryToProto(root_resource_id_, proto.mutable_drive_directory());
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

  // An old proto file might not have per-directory changestamps. Add them if
  // needed.
  const DriveDirectoryProto& root = proto.drive_directory();
  if (!root.drive_entry().directory_specific_info().has_changestamp()) {
    AddPerDirectoryChangestamps(proto.mutable_drive_directory(),
                                proto.largest_changestamp());
  }

  if (proto.drive_directory().drive_entry().resource_id() !=
      root_resource_id_) {
    LOG(ERROR) << "Incompatible proto detected (incompatible root ID): "
               << proto.drive_directory().drive_entry().resource_id();
    return false;
  }

  storage_->PutEntry(
      CreateEntryWithProperBaseName(proto.drive_directory().drive_entry()));
  AddDescendantsFromProto(proto.drive_directory());

  loaded_ = true;
  largest_changestamp_ = proto.largest_changestamp();

  return true;
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
  DriveEntryProto updated_entry(entry);

  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  SetBaseNameFromTitle(&updated_entry);

  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int modifier = 1;
  base::FilePath full_file_name(updated_entry.base_name());
  const std::string extension = full_file_name.Extension();
  const std::string file_name = full_file_name.RemoveExtension().value();
  while (!storage_->GetChild(entry.parent_resource_id(),
                             full_file_name.value()).empty()) {
    if (!extension.empty()) {
      full_file_name = base::FilePath(base::StringPrintf("%s (%d)%s",
                                                         file_name.c_str(),
                                                         ++modifier,
                                                         extension.c_str()));
    } else {
      full_file_name = base::FilePath(base::StringPrintf("%s (%d)",
                                                         file_name.c_str(),
                                                         ++modifier));
    }
  }
  updated_entry.set_base_name(full_file_name.value());

  // Setup child and parent links.
  storage_->PutChild(entry.parent_resource_id(),
                     updated_entry.base_name(),
                     updated_entry.resource_id());

  // Add the entry to resource map.
  storage_->PutEntry(updated_entry);
}

void DriveResourceMetadata::RemoveDirectoryChild(
    const std::string& child_resource_id) {
  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(child_resource_id);
  DCHECK(entry);
  DetachEntryFromDirectory(child_resource_id);
  if (entry->file_info().is_directory())
    RemoveDirectoryChildren(child_resource_id);
}

void DriveResourceMetadata::DetachEntryFromDirectory(
    const std::string& child_resource_id) {
  scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(child_resource_id);
  DCHECK(entry);

  // entry must be present in this directory.
  DCHECK_EQ(entry->resource_id(),
            storage_->GetChild(entry->parent_resource_id(),
                               entry->base_name()));
  // Remove entry from resource map first.
  storage_->RemoveEntry(entry->resource_id());

  // Then delete it from tree.
  storage_->RemoveChild(entry->parent_resource_id(), entry->base_name());
}

void DriveResourceMetadata::RemoveDirectoryChildren(
    const std::string& directory_resource_id) {
  std::vector<std::string> children;
  storage_->GetChildren(directory_resource_id, &children);
  for (size_t i = 0; i < children.size(); ++i)
    RemoveDirectoryChild(children[i]);
}

void DriveResourceMetadata::AddDescendantsFromProto(
    const DriveDirectoryProto& proto) {
  DCHECK(proto.drive_entry().file_info().is_directory());
  DCHECK(!proto.drive_entry().has_file_specific_info());

#if !defined(NDEBUG)
  std::vector<std::string> children;
  storage_->GetChildren(proto.drive_entry().resource_id(), &children);
  DCHECK(children.empty());
#endif

  // Add child files.
  for (int i = 0; i < proto.child_files_size(); ++i) {
    DriveEntryProto file(CreateEntryWithProperBaseName(proto.child_files(i)));
    DCHECK_EQ(proto.drive_entry().resource_id(), file.parent_resource_id());
    AddEntryToDirectory(file);
  }
  // Add child directories recursively.
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    DriveEntryProto child_dir(CreateEntryWithProperBaseName(
        proto.child_directories(i).drive_entry()));
    DCHECK_EQ(proto.drive_entry().resource_id(),
              child_dir.parent_resource_id());
    AddEntryToDirectory(child_dir);
    AddDescendantsFromProto(proto.child_directories(i));
  }
}

void DriveResourceMetadata::DirectoryToProto(
    const std::string& directory_resource_id,
    DriveDirectoryProto* proto) {
  scoped_ptr<DriveEntryProto> directory =
      storage_->GetEntry(directory_resource_id);
  DCHECK(directory);
  *proto->mutable_drive_entry() = *directory;
  DCHECK(proto->drive_entry().file_info().is_directory());

  std::vector<std::string> children;
  storage_->GetChildren(directory_resource_id, &children);
  for (size_t i = 0; i < children.size(); ++i) {
    scoped_ptr<DriveEntryProto> entry = storage_->GetEntry(children[i]);
    DCHECK(entry);
    if (entry->file_info().is_directory())
      DirectoryToProto(entry->resource_id(), proto->add_child_directories());
    else
      *proto->add_child_files() = *entry;
  }
}

scoped_ptr<DriveEntryProtoVector>
DriveResourceMetadata::DirectoryChildrenToProtoVector(
    const std::string& directory_resource_id) {
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
