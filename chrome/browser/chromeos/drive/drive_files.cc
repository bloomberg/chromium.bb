// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_files.h"

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
    resource_metadata_->AddEntryToDirectory(this, file.release());
  }
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    scoped_ptr<DriveDirectory> dir(resource_metadata_->CreateDriveDirectory());
    dir->FromProto(proto.child_directories(i));
    resource_metadata_->AddEntryToDirectory(this, dir.release());
  }
}

void DriveDirectory::ToProto(DriveDirectoryProto* proto) const {
  *proto->mutable_drive_entry() = proto_;
  DCHECK(proto->drive_entry().file_info().is_directory());

  const DriveResourceMetadata::ChildMap& children =
      resource_metadata_->child_map(resource_id());
  for (DriveResourceMetadata::ChildMap::const_iterator iter = children.begin();
       iter != children.end(); ++iter) {
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
  const DriveResourceMetadata::ChildMap& children =
      resource_metadata_->child_map(resource_id());
  for (DriveResourceMetadata::ChildMap::const_iterator iter = children.begin();
       iter != children.end(); ++iter) {
    const DriveEntryProto& proto =
        resource_metadata_->GetEntryByResourceId(iter->second)->proto();
    entries->push_back(proto);
  }
  return entries.Pass();
}

}  // namespace drive
