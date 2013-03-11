// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_files.h"

#include "base/stringprintf.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "net/base/escape.h"

namespace drive {

// DriveEntry class.

DriveEntry::DriveEntry(DriveResourceMetadata* resource_metadata)
    : resource_metadata_(resource_metadata) {
  DCHECK(resource_metadata);
}

DriveEntry::~DriveEntry() {
}

DriveDirectory* DriveEntry::AsDriveDirectory() {
  return NULL;
}

base::FilePath DriveEntry::GetFilePath() const {
  base::FilePath path;
  DriveEntry* parent = proto_.parent_resource_id().empty() ? NULL :
      resource_metadata_->GetEntryByResourceId(proto_.parent_resource_id());
  if (parent)
    path = parent->GetFilePath();
  path = path.Append(proto_.base_name());
  return path;
}

void DriveEntry::set_parent_resource_id(const std::string& parent_resource_id) {
  proto_.set_parent_resource_id(parent_resource_id);
}

void DriveEntry::SetBaseNameFromTitle() {
  if (proto_.has_file_specific_info() &&
      proto_.file_specific_info().is_hosted_document()) {
    proto_.set_base_name(util::EscapeUtf8FileName(
        proto_.title() + proto_.file_specific_info().document_extension()));
  } else {
    proto_.set_base_name(util::EscapeUtf8FileName(proto_.title()));
  }
}

// DriveDirectory class implementation.

DriveDirectory::DriveDirectory(DriveResourceMetadata* resource_metadata)
    : DriveEntry(resource_metadata) {
  proto_.mutable_file_info()->set_is_directory(true);
}

DriveDirectory::~DriveDirectory() {
  RemoveChildren();
}

int64 DriveDirectory::changestamp() const {
  DCHECK(proto_.has_directory_specific_info());
  return proto_.directory_specific_info().changestamp();
}

void DriveDirectory::set_changestamp(int64 changestamp) {
  DCHECK(proto_.has_directory_specific_info());
  proto_.mutable_directory_specific_info()->set_changestamp(changestamp);
}

DriveDirectory* DriveDirectory::AsDriveDirectory() {
  return this;
}

void DriveDirectory::AddEntry(DriveEntry* entry) {
  DCHECK(entry->parent_resource_id().empty() ||
         entry->parent_resource_id() == proto_.resource_id());

  // Try to add the entry to resource map.
  if (!resource_metadata_->AddEntryToResourceMap(entry)) {
    LOG(WARNING) << "Duplicate resource=" << entry->resource_id()
                 << ", title=" << entry->title();
    return;
  }

  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  entry->SetBaseNameFromTitle();

  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int max_modifier = 1;
  base::FilePath full_file_name(entry->base_name());
  const std::string extension = full_file_name.Extension();
  const std::string file_name = full_file_name.RemoveExtension().value();
  while (!FindChild(full_file_name.value()).empty()) {
    if (!extension.empty()) {
      full_file_name = base::FilePath(base::StringPrintf("%s (%d)%s",
                                                   file_name.c_str(),
                                                   ++max_modifier,
                                                   extension.c_str()));
    } else {
      full_file_name = base::FilePath(base::StringPrintf("%s (%d)",
                                                   file_name.c_str(),
                                                   ++max_modifier));
    }
  }
  entry->set_base_name(full_file_name.value());

  DVLOG(1) << "AddEntry: dir = " << GetFilePath().value()
           << ", file = " + entry->base_name()
           << ", parent resource = " << entry->parent_resource_id()
           << ", resource = " + entry->resource_id();

  // Setup child and parent links.
  if (entry->proto().file_info().is_directory()) {
    child_directories_.insert(std::make_pair(entry->base_name(),
                                             entry->resource_id()));
  } else {
    child_files_.insert(std::make_pair(entry->base_name(),
                                       entry->resource_id()));
  }
  entry->set_parent_resource_id(proto_.resource_id());
}

void DriveDirectory::TakeOverEntries(DriveDirectory* dir) {
  for (ChildMap::const_iterator iter = dir->child_files_.begin();
       iter != dir->child_files_.end(); ++iter) {
    TakeOverEntry(iter->second);
  }
  dir->child_files_.clear();

  for (ChildMap::iterator iter = dir->child_directories_.begin();
       iter != dir->child_directories_.end(); ++iter) {
    TakeOverEntry(iter->second);
  }
  dir->child_directories_.clear();
}

void DriveDirectory::TakeOverEntry(const std::string& resource_id) {
  DriveEntry* entry = resource_metadata_->GetEntryByResourceId(resource_id);
  DCHECK(entry);
  resource_metadata_->RemoveEntryFromResourceMap(resource_id);
  entry->set_parent_resource_id(std::string());
  AddEntry(entry);
}

void DriveDirectory::RemoveEntry(DriveEntry* entry) {
  DCHECK(entry);

  RemoveChild(entry);
  delete entry;
}

std::string DriveDirectory::FindChild(
    const base::FilePath::StringType& file_name) const {
  ChildMap::const_iterator iter = child_files_.find(file_name);
  if (iter != child_files_.end())
    return iter->second;

  iter = child_directories_.find(file_name);
  if (iter != child_directories_.end())
    return iter->second;

  return std::string();
}

void DriveDirectory::RemoveChild(DriveEntry* entry) {
  DCHECK(entry);

  const std::string& base_name(entry->base_name());
  // entry must be present in this directory.
  DCHECK_EQ(entry->resource_id(), FindChild(base_name));
  // Remove entry from resource map first.
  resource_metadata_->RemoveEntryFromResourceMap(entry->resource_id());

  // Then delete it from tree.
  child_files_.erase(base_name);
  child_directories_.erase(base_name);

  entry->set_parent_resource_id(std::string());
}

void DriveDirectory::RemoveChildren() {
  RemoveChildFiles();
  RemoveChildDirectories();
}

void DriveDirectory::RemoveChildFiles() {
  DVLOG(1) << "RemoveChildFiles " << proto_.resource_id();
  for (ChildMap::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    DriveEntry* child = resource_metadata_->GetEntryByResourceId(iter->second);
    DCHECK(child);
    resource_metadata_->RemoveEntryFromResourceMap(iter->second);
    delete child;
  }
  child_files_.clear();
}

void DriveDirectory::RemoveChildDirectories() {
  for (ChildMap::iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    DriveDirectory* dir = resource_metadata_->GetEntryByResourceId(
        iter->second)->AsDriveDirectory();
    DCHECK(dir);
    // Remove directories recursively.
    dir->RemoveChildren();
    resource_metadata_->RemoveEntryFromResourceMap(iter->second);
    delete dir;
  }
  child_directories_.clear();
}

void DriveDirectory::GetChildDirectoryPaths(
    std::set<base::FilePath>* child_dirs) {
  for (ChildMap::const_iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    DriveDirectory* dir = resource_metadata_->GetEntryByResourceId(
        iter->second)->AsDriveDirectory();
    DCHECK(dir);
    child_dirs->insert(dir->GetFilePath());
    dir->GetChildDirectoryPaths(child_dirs);
  }
}

// Convert to/from proto.

void DriveEntry::FromProto(const DriveEntryProto& proto) {
  proto_ = proto;
  SetBaseNameFromTitle();
}

void DriveDirectory::FromProto(const DriveDirectoryProto& proto) {
  DCHECK(proto.drive_entry().file_info().is_directory());
  DCHECK(!proto.drive_entry().has_file_specific_info());

  DriveEntry::FromProto(proto.drive_entry());

  for (int i = 0; i < proto.child_files_size(); ++i) {
    scoped_ptr<DriveEntry> file(resource_metadata_->CreateDriveEntry());
    file->FromProto(proto.child_files(i));
    AddEntry(file.release());
  }
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    scoped_ptr<DriveDirectory> dir(resource_metadata_->CreateDriveDirectory());
    dir->FromProto(proto.child_directories(i));
    AddEntry(dir.release());
  }
}

void DriveDirectory::ToProto(DriveDirectoryProto* proto) const {
  *proto->mutable_drive_entry() = proto_;
  DCHECK(proto->drive_entry().file_info().is_directory());

  for (ChildMap::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    DriveEntry* file = resource_metadata_->GetEntryByResourceId(
        iter->second);
    DCHECK(file);
    DCHECK(!file->proto().file_info().is_directory());
    *proto->add_child_files() = file->proto();
  }
  for (ChildMap::const_iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    DriveDirectory* dir = resource_metadata_->GetEntryByResourceId(
        iter->second)->AsDriveDirectory();
    DCHECK(dir);
    dir->ToProto(proto->add_child_directories());
  }
}

scoped_ptr<DriveEntryProtoVector> DriveDirectory::ToProtoVector() const {
  scoped_ptr<DriveEntryProtoVector> entries(new DriveEntryProtoVector);
  // Use ToProtoFull, as we don't want to include children in |proto|.
  for (ChildMap::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    const DriveEntryProto& proto =
        resource_metadata_->GetEntryByResourceId(iter->second)->proto();
    entries->push_back(proto);
  }
  for (ChildMap::const_iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    const DriveEntryProto& proto =
        resource_metadata_->GetEntryByResourceId(iter->second)->proto();
    entries->push_back(proto);
  }

  return entries.Pass();
}

}  // namespace drive
