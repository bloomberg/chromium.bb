// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_files.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "net/base/escape.h"

namespace drive {

// DriveEntry class.

DriveEntry::DriveEntry() {
}

DriveEntry::~DriveEntry() {
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

int64 DriveEntry::changestamp() const {
  DCHECK(proto_.file_info().is_directory());
  return proto_.directory_specific_info().changestamp();
}

void DriveEntry::set_changestamp(int64 changestamp) {
  DCHECK(proto_.file_info().is_directory());
  proto_.mutable_directory_specific_info()->set_changestamp(changestamp);
}

// Convert to/from proto.

void DriveEntry::FromProto(const DriveEntryProto& proto) {
  proto_ = proto;
  SetBaseNameFromTitle();
}

}  // namespace drive
