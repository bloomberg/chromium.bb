// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/drive_operations.h"

#include "chrome/browser/chromeos/drive/file_system/move_operation.h"
#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"

namespace drive {
namespace file_system {

DriveOperations::DriveOperations() {
}

DriveOperations::~DriveOperations() {
}

void DriveOperations::Init(MoveOperation* move_operation,
                           RemoveOperation* remove_operation) {
  move_operation_.reset(move_operation);
  remove_operation_.reset(remove_operation);

}

void DriveOperations::Move(const FilePath& src_file_path,
                           const FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  move_operation_->Move(src_file_path, dest_file_path, callback);
}

void DriveOperations::Remove(const FilePath& file_path,
                             bool is_recursive,
                             const FileOperationCallback& callback) {
  remove_operation_->Remove(file_path, is_recursive, callback);
}
}  // namespace file_system
}  // namespace drive
