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
      serialized_size_(0),
      largest_changestamp_(0),
      loaded_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DriveEntryProto root_info;
  root_info.mutable_file_info()->set_is_directory(true);
  root_info.set_resource_id(root_resource_id);
  root_info.set_title(kDriveRootDirectory);
  root_.reset(new DriveEntryProto(CreateEntryWithProperBaseName(root_info)));

  AddEntryToResourceMap(root_.get());
}

DriveResourceMetadata::~DriveResourceMetadata() {
  ClearRoot();
}

void DriveResourceMetadata::ClearRoot() {
  if (!root_.get())
    return;

  // The root is not yet initialized.
  if (root_->resource_id().empty())
    return;

  // Note that children have a reference to root_,
  // so we need to delete them here.
  RemoveDirectoryChildren(root_.get());
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

  DriveEntryProto* entry = FindEntryByPathSync(file_path);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DriveEntryProto* destination = FindEntryByPathSync(directory_path);
  base::FilePath moved_file_path;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  if (!destination) {
    error = DRIVE_FILE_ERROR_NOT_FOUND;
  } else if (!destination->file_info().is_directory()) {
    error = DRIVE_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    DriveEntryProto* parent = entry->parent_resource_id().empty() ? NULL :
        GetEntryByResourceId(entry->parent_resource_id());
    if (parent && parent->file_info().is_directory())
      DetachEntryFromDirectory(parent, entry);

    AddEntryToDirectory(destination, entry);
    moved_file_path = GetFilePath(*entry);
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
  DriveEntryProto* entry = FindEntryByPathSync(file_path);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  if (new_name == file_path.BaseName().value()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_EXISTS);
    return;
  }

  entry->set_title(new_name);

  DriveEntryProto* parent = GetEntryByResourceId(entry->parent_resource_id());
  DCHECK(parent);
  // After changing the title of the entry, call MoveEntryToDirectory to
  // remove the entry from its parent directory and then add it back in order to
  // go through the file name de-duplication.
  // TODO(achuith/satorux/zel): This code is fragile. The title has been
  // changed, but not the file_name. MoveEntryToDirectory calls RemoveChild to
  // remove the child based on the old file_name, and then re-adds the child by
  // first assigning the new title to file_name. http://crbug.com/30157
  MoveEntryToDirectory(file_path, GetFilePath(*parent), callback);
}

void DriveResourceMetadata::RemoveEntry(const std::string& resource_id,
                                        const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Disallow deletion of root.
  if (resource_id == root_->resource_id()) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_ACCESS_DENIED);
    return;
  }

  DriveEntryProto* entry = GetEntryByResourceId(resource_id);
  if (!entry) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DriveEntryProto* parent = GetEntryByResourceId(entry->parent_resource_id());
  DCHECK(parent && parent->file_info().is_directory());

  RemoveDirectoryChild(parent, entry);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK, GetFilePath(*parent)));
}

bool DriveResourceMetadata::AddEntryToResourceMap(DriveEntryProto* entry) {
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

DriveEntryProto* DriveResourceMetadata::FindEntryByPathSync(
    const base::FilePath& file_path) {
  if (file_path == GetFilePath(*root_))
    return root_.get();

  std::vector<base::FilePath::StringType> components;
  file_path.GetComponents(&components);
  DriveEntryProto* current_dir = root_.get();

  for (size_t i = 1; i < components.size() && current_dir; ++i) {
    std::string resource_id = FindDirectoryChild(current_dir, components[i]);
    if (resource_id.empty())
      return NULL;

    DriveEntryProto* entry = GetEntryByResourceId(resource_id);
    DCHECK(entry);

    if (i == components.size() - 1)  // Last component.
      return entry;
    if (!entry->file_info().is_directory())
      return NULL;
    current_dir = entry;
  }
  return NULL;
}

DriveEntryProto* DriveResourceMetadata::GetEntryByResourceId(
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

  DriveEntryProto* entry = GetEntryByResourceId(resource_id);
  if (entry) {
    entry_proto.reset(new DriveEntryProto(*entry));
    error = DRIVE_FILE_OK;
    drive_file_path = GetFilePath(*entry);
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

  DriveEntryProto* entry = FindEntryByPathSync(path);
  if (entry) {
    entry_proto.reset(new DriveEntryProto(*entry));
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

  DriveEntryProto* entry = FindEntryByPathSync(path);
  if (entry && entry->file_info().is_directory()) {
    entries = DirectoryChildrenToProtoVector(entry);
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

  DriveEntryProto* entry = GetEntryByResourceId(entry_proto.resource_id());
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
  if (entry != root_.get()) {
    DriveEntryProto* old_parent = GetDirectory(entry->parent_resource_id());
    DriveEntryProto* new_parent =
        GetDirectory(entry_proto.parent_resource_id());

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
    DetachEntryFromDirectory(old_parent, entry);
    // Note that it's safe to update the directory entry with
    // DriveEntry::FromProto() as it won't clear children.
    *entry = CreateEntryWithProperBaseName(entry_proto);
    AddEntryToDirectory(new_parent, entry);  // Transfers ownership.
  } else {
    // root has no parent.
    *entry = CreateEntryWithProperBaseName(entry_proto);
  }

  DVLOG(1) << "RefreshEntry " << GetFilePath(*entry).value();
  // Note that base_name is not the same for new_entry and entry_proto.
  scoped_ptr<DriveEntryProto> result_entry_proto(new DriveEntryProto(*entry));
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 DRIVE_FILE_OK,
                 GetFilePath(*entry),
                 base::Passed(&result_entry_proto)));
}

void DriveResourceMetadata::RefreshDirectory(
    const DirectoryFetchInfo& directory_fetch_info,
    const DriveEntryProtoMap& entry_proto_map,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!directory_fetch_info.empty());

  DriveEntryProto* directory = GetEntryByResourceId(
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

  // First, go through the entry map. We'll handle existing entries and new
  // entries in the loop. We'll process deleted entries afterwards.
  for (DriveEntryProtoMap::const_iterator it = entry_proto_map.begin();
       it != entry_proto_map.end(); ++it) {
    const DriveEntryProto& entry_proto = it->second;
    DriveEntryProto* existing_entry =
        GetEntryByResourceId(entry_proto.resource_id());
    if (existing_entry) {
      DriveEntryProto* old_parent =
          GetDirectory(existing_entry->parent_resource_id());
      DCHECK(old_parent && old_parent->file_info().is_directory());
      // See the comment in RefreshEntry() for why the existing entry is
      // updated in this way.
      DetachEntryFromDirectory(old_parent, existing_entry);
      *existing_entry = CreateEntryWithProperBaseName(entry_proto);
      AddEntryToDirectory(directory, existing_entry);
    } else {  // New entry.
      // A new directory will have changestamp of zero, so the directory will
      // be "fast-fetched". See crbug.com/178348 for details.
      scoped_ptr<DriveEntryProto> new_entry(
          new DriveEntryProto(CreateEntryWithProperBaseName(entry_proto)));
      AddEntryToDirectory(directory, new_entry.release());
    }
  }

  // Go through the existing entries and remove deleted entries.
  scoped_ptr<DriveEntryProtoVector> entries =
      DirectoryChildrenToProtoVector(directory);
  for (size_t i = 0; i < entries->size(); ++i) {
    const DriveEntryProto& entry_proto = entries->at(i);
    if (entry_proto_map.count(entry_proto.resource_id()) == 0) {
      DriveEntryProto* entry = GetEntryByResourceId(entry_proto.resource_id());
      RemoveDirectoryChild(directory, entry);
    }
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK, GetFilePath(*directory)));
}

void DriveResourceMetadata::AddEntry(const DriveEntryProto& entry_proto,
                                     const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveEntryProto* parent = GetDirectory(entry_proto.parent_resource_id());
  if (!parent) {
    PostFileMoveCallbackError(callback, DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DriveEntryProto* added_entry =
      new DriveEntryProto(CreateEntryWithProperBaseName(entry_proto));
  AddEntryToDirectory(parent, added_entry);  // Transfers ownership.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DRIVE_FILE_OK, GetFilePath(*added_entry)));
}

DriveEntryProto* DriveResourceMetadata::GetDirectory(
    const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());

  DriveEntryProto* entry = GetEntryByResourceId(resource_id);
  return entry && entry->file_info().is_directory() ? entry : NULL;
}

base::FilePath DriveResourceMetadata::GetFilePath(
    const DriveEntryProto& entry) {
  base::FilePath path;
  DriveEntryProto* parent = entry.parent_resource_id().empty() ? NULL :
      GetEntryByResourceId(entry.parent_resource_id());
  if (parent)
    path = GetFilePath(*parent);
  path = path.Append(entry.base_name());
  return path;
}

void DriveResourceMetadata::GetChildDirectories(
    const std::string& resource_id,
    const GetChildDirectoriesCallback& changed_dirs_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!changed_dirs_callback.is_null());

  std::set<base::FilePath> changed_directories;
  DriveEntryProto* entry = GetEntryByResourceId(resource_id);
  DriveEntryProto* directory =
      entry && entry->file_info().is_directory() ? entry : NULL;
  if (directory)
    GetDescendantDirectoryPaths(*directory, &changed_directories);

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(changed_dirs_callback, changed_directories));
}

void DriveResourceMetadata::GetDescendantDirectoryPaths(
    const DriveEntryProto& directory,
    std::set<base::FilePath>* child_directories) {
  DCHECK(directory.file_info().is_directory());
  const ChildMap& children = child_maps_[directory.resource_id()];
  for (ChildMap::const_iterator iter = children.begin();
       iter != children.end(); ++iter) {
    DriveEntryProto* entry = GetEntryByResourceId(iter->second);
    if (entry && entry->file_info().is_directory()) {
      child_directories->insert(GetFilePath(*entry));
      GetDescendantDirectoryPaths(*entry, child_directories);
    }
  }
}

void DriveResourceMetadata::RemoveAll(const base::Closure& callback) {
  RemoveDirectoryChildren(root_.get());
  base::MessageLoopProxy::current()->PostTask(FROM_HERE, callback);
}

void DriveResourceMetadata::SerializeToString(std::string* serialized_proto) {
  DriveRootDirectoryProto proto;
  DirectoryToProto(root_.get(), proto.mutable_drive_directory());
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

  ProtoToDirectory(proto.drive_directory(), root_.get());

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

void DriveResourceMetadata::AddEntryToDirectory(DriveEntryProto* directory,
                                                DriveEntryProto* entry) {
  DCHECK(directory->file_info().is_directory());
  DCHECK(entry->parent_resource_id().empty() ||
         entry->parent_resource_id() == directory->resource_id());

  // Try to add the entry to resource map.
  if (!AddEntryToResourceMap(entry)) {
    LOG(WARNING) << "Duplicate resource=" << entry->resource_id()
                 << ", title=" << entry->title();
    return;
  }

  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  SetBaseNameFromTitle(entry);

  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int modifier = 1;
  base::FilePath full_file_name(entry->base_name());
  const std::string extension = full_file_name.Extension();
  const std::string file_name = full_file_name.RemoveExtension().value();
  while (!FindDirectoryChild(directory, full_file_name.value()).empty()) {
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
  entry->set_base_name(full_file_name.value());

  // Setup child and parent links.
  child_maps_[directory->resource_id()].insert(
      std::make_pair(entry->base_name(), entry->resource_id()));
  entry->set_parent_resource_id(directory->resource_id());
}

void DriveResourceMetadata::RemoveDirectoryChild(DriveEntryProto* directory,
                                                 DriveEntryProto* entry) {
  DCHECK(directory->file_info().is_directory());
  DetachEntryFromDirectory(directory, entry);
  if (entry->file_info().is_directory())
    RemoveDirectoryChildren(entry);
  delete entry;
}

std::string DriveResourceMetadata::FindDirectoryChild(
    DriveEntryProto* directory,
    const base::FilePath::StringType& file_name) {
  DCHECK(directory->file_info().is_directory());
  const ChildMap& children = child_maps_[directory->resource_id()];
  DriveResourceMetadata::ChildMap::const_iterator iter =
      children.find(file_name);
  if (iter != children.end())
    return iter->second;
  return std::string();
}

void DriveResourceMetadata::DetachEntryFromDirectory(
    DriveEntryProto* directory,
    DriveEntryProto* entry) {
  DCHECK(directory->file_info().is_directory());
  DCHECK(entry);

  const std::string& base_name(entry->base_name());
  // entry must be present in this directory.
  DCHECK_EQ(entry->resource_id(), FindDirectoryChild(directory, base_name));
  // Remove entry from resource map first.
  RemoveEntryFromResourceMap(entry->resource_id());

  // Then delete it from tree.
  child_maps_[directory->resource_id()].erase(base_name);

  entry->set_parent_resource_id(std::string());
}

void DriveResourceMetadata::RemoveDirectoryChildren(
    DriveEntryProto* directory) {
  DCHECK(directory->file_info().is_directory());
  DriveResourceMetadata::ChildMap* children =
      &child_maps_[directory->resource_id()];
  for (DriveResourceMetadata::ChildMap::iterator iter = children->begin();
       iter != children->end(); ++iter) {
    DriveEntryProto* child = GetEntryByResourceId(iter->second);
    DCHECK(child);
    // Remove directories recursively.
    if (child->file_info().is_directory())
      RemoveDirectoryChildren(child);

    RemoveEntryFromResourceMap(iter->second);
    delete child;
  }
  child_maps_.erase(directory->resource_id());
}

void DriveResourceMetadata::ProtoToDirectory(const DriveDirectoryProto& proto,
                                             DriveEntryProto* directory) {
  DCHECK(proto.drive_entry().file_info().is_directory());
  DCHECK(!proto.drive_entry().has_file_specific_info());

  *directory = CreateEntryWithProperBaseName(proto.drive_entry());

  for (int i = 0; i < proto.child_files_size(); ++i) {
    scoped_ptr<DriveEntryProto> file(new DriveEntryProto(
        CreateEntryWithProperBaseName(proto.child_files(i))));
    AddEntryToDirectory(directory, file.release());
  }
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    scoped_ptr<DriveEntryProto> child_dir(new DriveEntryProto);
    ProtoToDirectory(proto.child_directories(i), child_dir.get());
    AddEntryToDirectory(directory, child_dir.release());
  }
}

void DriveResourceMetadata::DirectoryToProto(DriveEntryProto* directory,
                                             DriveDirectoryProto* proto) {
  *proto->mutable_drive_entry() = *directory;
  DCHECK(proto->drive_entry().file_info().is_directory());

  const ChildMap& children = child_maps_[directory->resource_id()];
  for (ChildMap::const_iterator iter = children.begin();
       iter != children.end(); ++iter) {
    DriveEntryProto* entry = GetEntryByResourceId(iter->second);
    DCHECK(entry);
    if (entry->file_info().is_directory())
      DirectoryToProto(entry, proto->add_child_directories());
    else
      *proto->add_child_files() = *entry;
  }
}

scoped_ptr<DriveEntryProtoVector>
DriveResourceMetadata::DirectoryChildrenToProtoVector(
    DriveEntryProto* directory) {
  DCHECK(directory->file_info().is_directory());
  scoped_ptr<DriveEntryProtoVector> entries(new DriveEntryProtoVector);
  const ChildMap& children = child_maps_[directory->resource_id()];
  for (ChildMap::const_iterator iter = children.begin();
       iter != children.end(); ++iter) {
    const DriveEntryProto& proto = *GetEntryByResourceId(iter->second);
    entries->push_back(proto);
  }
  return entries.Pass();
}

}  // namespace drive
