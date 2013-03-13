// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_files.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
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

DriveDirectory::DriveDirectory() {
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

}  // namespace drive
