// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"

#include <leveldb/db.h>
#include <stack>
#include <utility>

#include "base/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_files.h"
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
      serialized_size_(0),
      largest_changestamp_(0),
      loaded_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  root_ = CreateDriveDirectory().Pass();
  root_->set_resource_id(root_resource_id);
  root_->set_title(kDriveRootDirectory);
  root_->SetBaseNameFromTitle();

  AddEntryToResourceMap(root_.get());
}

DriveResourceMetadata::~DriveResourceMetadata() {
  ClearRoot();
}

scoped_ptr<DriveEntry> DriveResourceMetadata::CreateDriveEntry() {
  return scoped_ptr<DriveEntry>(new DriveEntry());
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

  DriveEntry* entry = FindEntryByPathSync(file_path);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DriveEntry* destination = FindEntryByPathSync(directory_path);
  base::FilePath moved_file_path;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  if (!destination) {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  } else if (!destination->AsDriveDirectory()) {
    error = DRIVE_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    DriveDirectory* parent =
        entry->parent_resource_id().empty() ? NULL :
        GetEntryByResourceId(entry->parent_resource_id())->AsDriveDirectory();
    if (parent)
      parent->RemoveChild(entry);

    destination->AsDriveDirectory()->AddEntry(entry);
    moved_file_path = GetFilePath(entry->proto());
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

  DriveEntry* parent = GetEntryByResourceId(entry->parent_resource_id());
  DCHECK(parent);
  // After changing the title of the entry, call MoveEntryToDirectory to
  // remove the entry from its parent directory and then add it back in order to
  // go through the file name de-duplication.
  // TODO(achuith/satorux/zel): This code is fragile. The title has been
  // changed, but not the file_name. MoveEntryToDirectory calls RemoveChild to
  // remove the child based on the old file_name, and then re-adds the child by
  // first assigning the new title to file_name. http://crbug.com/30157
  MoveEntryToDirectory(file_path, GetFilePath(parent->proto()), callback);
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

  DriveDirectory* parent =
      GetEntryByResourceId(entry->parent_resource_id())->AsDriveDirectory();
  DCHECK(parent);

  DVLOG(1) << "RemoveEntryFromParent " << GetFilePath(entry->proto()).value();
  parent->RemoveEntry(entry);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK, GetFilePath(parent->proto())));
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
    const base::FilePath& file_path) {
  if (file_path == GetFilePath(root_->proto()))
    return root_.get();

  std::vector<base::FilePath::StringType> components;
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
  base::FilePath drive_file_path;

  DriveEntry* entry = GetEntryByResourceId(resource_id);
  if (entry) {
    entry_proto.reset(new DriveEntryProto(entry->proto()));
    error = DRIVE_FILE_OK;
    drive_file_path = GetFilePath(entry->proto());
  } else {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, drive_file_path, base::Passed(&entry_proto)));
}

void DriveResourceMetadata::GetEntryInfoByPath(
    const base::FilePath& path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> entry_proto;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;

  DriveEntry* entry = FindEntryByPathSync(path);
  if (entry) {
    entry_proto.reset(new DriveEntryProto(entry->proto()));
    error = DRIVE_FILE_OK;
  } else {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&entry_proto)));
}

void DriveResourceMetadata::ReadDirectoryByPath(
    const base::FilePath& path,
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

  DriveEntry* entry = GetEntryByResourceId(entry_proto.resource_id());
  if (!entry) {
    PostGetEntryInfoWithFilePathCallbackError(
        callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  // Reject incompatible input.
  if (entry->proto().file_info().is_directory() !=
      entry_proto.file_info().is_directory()) {
    PostGetEntryInfoWithFilePathCallbackError(
        callback, DRIVE_FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // Update data.
  if (entry != root_.get()) {
    DriveDirectory* old_parent = GetDirectory(entry->parent_resource_id());
    DriveDirectory* new_parent = GetDirectory(entry_proto.parent_resource_id());

    if (!old_parent || !new_parent) {
      PostGetEntryInfoWithFilePathCallbackError(
          callback, DRIVE_FILE_ERROR_NOT_FOUND);
      return;
    }

    // Remove from the old parent, update the entry, and add it to the new
    // parent. The order matters here. FromProto() could remove suffix like
    // "(2)" from entries with duplicate names. If FromProto() is first
    // called, RemoveChild() won't work correctly because of the missing
    // suffix.
    old_parent->RemoveChild(entry);
    // Note that it's safe to update the directory entry with
    // DriveEntry::FromProto() as it won't clear children.
    entry->FromProto(entry_proto);
    new_parent->AddEntry(entry);  // Transfers ownership.
  } else {
    // root has no parent.
    entry->FromProto(entry_proto);
  }

  DVLOG(1) << "RefreshEntry " << GetFilePath(entry->proto()).value();
  // Note that base_name is not the same for new_entry and entry_proto.
  scoped_ptr<DriveEntryProto> result_entry_proto(
      new DriveEntryProto(entry->proto()));
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 DRIVE_FILE_OK,
                 GetFilePath(entry->proto()),
                 base::Passed(&result_entry_proto)));
}

void DriveResourceMetadata::RefreshDirectory(
    const DirectoryFetchInfo& directory_fetch_info,
    const DriveEntryProtoMap& entry_proto_map,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!directory_fetch_info.empty());

  DriveEntry* directory_entry = GetEntryByResourceId(
      directory_fetch_info.resource_id());

  if (!directory_entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DriveDirectory* directory = directory_entry->AsDriveDirectory();
  if (!directory) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  directory->set_changestamp(directory_fetch_info.changestamp());

  // First, go through the entry map. We'll handle existing entries and new
  // entries in the loop. We'll process deleted entries afterwards.
  for (DriveEntryProtoMap::const_iterator it = entry_proto_map.begin();
       it != entry_proto_map.end(); ++it) {
    const DriveEntryProto& entry_proto = it->second;
    DriveEntry* existing_entry =
        GetEntryByResourceId(entry_proto.resource_id());
    if (existing_entry) {
      DriveDirectory* old_parent =
          GetDirectory(existing_entry->parent_resource_id());
      DCHECK(old_parent);
      // See the comment in RefreshEntry() for why the existing entry is
      // updated in this way.
      old_parent->RemoveChild(existing_entry);
      existing_entry->FromProto(entry_proto);
      directory->AddEntry(existing_entry);
    } else {  // New entry.
      // A new directory will have changestamp of zero, so the directory will
      // be "fast-fetched". See crbug.com/178348 for details.
      directory->AddEntry(CreateDriveEntryFromProto(entry_proto).release());
    }
  }

  // Go through the existing entries and remove deleted entries.
  scoped_ptr<DriveEntryProtoVector> entries =
      directory->AsDriveDirectory()->ToProtoVector();
  for (size_t i = 0; i < entries->size(); ++i) {
    const DriveEntryProto& entry_proto = entries->at(i);
    if (entry_proto_map.count(entry_proto.resource_id()) == 0) {
      DriveEntry* entry = GetEntryByResourceId(entry_proto.resource_id());
      directory->RemoveEntry(entry);
    }
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK, GetFilePath(directory->proto())));
}

void DriveResourceMetadata::AddEntry(const DriveEntryProto& entry_proto,
                                     const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveDirectory* parent = GetDirectory(entry_proto.parent_resource_id());
  if (!parent) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  scoped_ptr<DriveEntry> new_entry = CreateDriveEntryFromProto(entry_proto);
  if (!new_entry.get()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_FAILED);
    return;
  }

  DriveEntry* added_entry = new_entry.release();
  parent->AddEntry(added_entry);  // Transfers ownership.
  DVLOG(1) << "AddEntry "  << GetFilePath(added_entry->proto()).value();
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK, GetFilePath(added_entry->proto())));
}

DriveDirectory* DriveResourceMetadata::GetDirectory(
    const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());

  DriveEntry* entry = GetEntryByResourceId(resource_id);
  return entry ? entry->AsDriveDirectory() : NULL;
}

base::FilePath DriveResourceMetadata::GetFilePath(
    const DriveEntryProto& entry) {
  base::FilePath path;
  DriveEntry* parent = entry.parent_resource_id().empty() ? NULL :
      GetEntryByResourceId(entry.parent_resource_id());
  if (parent)
    path = GetFilePath(parent->proto());
  path = path.Append(entry.base_name());
  return path;
}

void DriveResourceMetadata::GetChildDirectories(
    const std::string& resource_id,
    const GetChildDirectoriesCallback& changed_dirs_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!changed_dirs_callback.is_null());

  std::set<base::FilePath> changed_directories;
  DriveEntry* entry = GetEntryByResourceId(resource_id);
  DriveDirectory* directory = entry ? entry->AsDriveDirectory() : NULL;
  if (directory)
    GetDescendantDirectoryPaths(*directory, &changed_directories);

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(changed_dirs_callback, changed_directories));
}

void DriveResourceMetadata::GetDescendantDirectoryPaths(
    const DriveDirectory& directory,
    std::set<base::FilePath>* child_directories) {
  const ChildMap& children = child_maps_[directory.resource_id()];
  for (ChildMap::const_iterator iter = children.begin();
       iter != children.end(); ++iter) {
    DriveDirectory* dir =
        GetEntryByResourceId(iter->second)->AsDriveDirectory();
    if (dir) {
      child_directories->insert(GetFilePath(dir->proto()));
      GetDescendantDirectoryPaths(*dir, child_directories);
    }
  }
}

void DriveResourceMetadata::RemoveAll(const base::Closure& callback) {
  root_->RemoveChildren();
  base::MessageLoopProxy::current()->PostTask(FROM_HERE, callback);
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

  // An old proto file might not have per-directory changestamps. Add them if
  // needed.
  const DriveDirectoryProto& root = proto.drive_directory();
  if (!root.drive_entry().directory_specific_info().has_changestamp()) {
    AddPerDirectoryChangestamps(proto.mutable_drive_directory(),
                                proto.largest_changestamp());
  }

  root_->FromProto(proto.drive_directory());

  loaded_ = true;
  largest_changestamp_ = proto.largest_changestamp();

  return true;
}

scoped_ptr<DriveEntry> DriveResourceMetadata::CreateDriveEntryFromProto(
    const DriveEntryProto& entry_proto) {
  // TODO(achuith): This method never fails. Add basic sanity checks for
  // resource_id, etc.
  scoped_ptr<DriveEntry> entry = entry_proto.file_info().is_directory() ?
      scoped_ptr<DriveEntry>(CreateDriveDirectory()) : CreateDriveEntry();
  entry->FromProto(entry_proto);
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

}  // namespace drive
