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

DriveEntry::DriveEntry() {
}

DriveEntry::~DriveEntry() {
}

DriveDirectory* DriveEntry::AsDriveDirectory() {
  return NULL;
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
    : resource_metadata_(resource_metadata) {
  proto_.mutable_file_info()->set_is_directory(true);
}

DriveDirectory::~DriveDirectory() {
  RemoveChildren();
}

int64 DriveDirectory::changestamp() const {
  return proto_.directory_specific_info().changestamp();
}

void DriveDirectory::set_changestamp(int64 changestamp) {
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

  // Setup child and parent links.
  children_.insert(std::make_pair(entry->base_name(),
                                  entry->resource_id()));
  entry->set_parent_resource_id(proto_.resource_id());
}

void DriveDirectory::RemoveEntry(DriveEntry* entry) {
  DCHECK(entry);

  RemoveChild(entry);
  delete entry;
}

std::string DriveDirectory::FindChild(
    const base::FilePath::StringType& file_name) const {
  ChildMap::const_iterator iter = children_.find(file_name);
  if (iter != children_.end())
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
  children_.erase(base_name);

  entry->set_parent_resource_id(std::string());
}

void DriveDirectory::RemoveChildren() {
  RemoveChildFiles();
  RemoveChildDirectories();
  DCHECK(children_.empty());
}

void DriveDirectory::RemoveChildFiles() {
  DVLOG(1) << "RemoveChildFiles " << proto_.resource_id();
  for (ChildMap::iterator iter = children_.begin(), iter_next = iter;
       iter != children_.end(); iter = iter_next) {
    ++iter_next;
    DriveEntry* child = resource_metadata_->GetEntryByResourceId(iter->second);
    DCHECK(child);
    if (!child->proto().file_info().is_directory()) {
      resource_metadata_->RemoveEntryFromResourceMap(iter->second);
      delete child;
      children_.erase(iter);
    }
  }
}

void DriveDirectory::RemoveChildDirectories() {
  for (ChildMap::iterator iter = children_.begin(), iter_next = iter;
       iter != children_.end(); iter = iter_next) {
    ++iter_next;
    DriveDirectory* dir = resource_metadata_->GetEntryByResourceId(
        iter->second)->AsDriveDirectory();
    if (dir) {
      // Remove directories recursively.
      dir->RemoveChildren();
      resource_metadata_->RemoveEntryFromResourceMap(iter->second);
      delete dir;
      children_.erase(iter);
    }
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

  for (ChildMap::const_iterator iter = children_.begin();
       iter != children_.end(); ++iter) {
    DriveEntry* entry = resource_metadata_->GetEntryByResourceId(
        iter->second);
    DCHECK(entry);
    DriveDirectory* dir = entry->AsDriveDirectory();
    if (dir)
      dir->ToProto(proto->add_child_directories());
    else
      *proto->add_child_files() = entry->proto();
  }
}

scoped_ptr<DriveEntryProtoVector> DriveDirectory::ToProtoVector() const {
  scoped_ptr<DriveEntryProtoVector> entries(new DriveEntryProtoVector);
  for (ChildMap::const_iterator iter = children_.begin();
       iter != children_.end(); ++iter) {
    const DriveEntryProto& proto =
        resource_metadata_->GetEntryByResourceId(iter->second)->proto();
    entries->push_back(proto);
  }
  return entries.Pass();
}

}  // namespace drive
