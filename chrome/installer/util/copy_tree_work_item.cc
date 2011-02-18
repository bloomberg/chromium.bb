// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/copy_tree_work_item.h"

#include <shlwapi.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/installer/util/logging_installer.h"

CopyTreeWorkItem::~CopyTreeWorkItem() {
}

CopyTreeWorkItem::CopyTreeWorkItem(const FilePath& source_path,
                                   const FilePath& dest_path,
                                   const FilePath& temp_dir,
                                   CopyOverWriteOption overwrite_option,
                                   const FilePath& alternative_path)
    : source_path_(source_path),
      dest_path_(dest_path),
      temp_dir_(temp_dir),
      overwrite_option_(overwrite_option),
      alternative_path_(alternative_path),
      copied_to_dest_path_(false),
      moved_to_backup_(false),
      copied_to_alternate_path_(false) {
}

bool CopyTreeWorkItem::Do() {
  if (!file_util::PathExists(source_path_)) {
    LOG(ERROR) << source_path_.value() << " does not exist";
    return false;
  }

  bool dest_exist = file_util::PathExists(dest_path_);
  // handle overwrite_option_ = IF_DIFFERENT case.
  if ((dest_exist) &&
      (overwrite_option_ == WorkItem::IF_DIFFERENT) &&  // only for single file
      (!file_util::DirectoryExists(source_path_)) &&
      (!file_util::DirectoryExists(dest_path_)) &&
      (file_util::ContentsEqual(source_path_, dest_path_))) {
    VLOG(1) << "Source file " << source_path_.value()
            << " and destination file " << dest_path_.value()
            << " are exactly same. Returning true.";
    return true;
  } else if ((dest_exist) &&
             (overwrite_option_ == WorkItem::NEW_NAME_IF_IN_USE) &&
             (!file_util::DirectoryExists(source_path_)) &&
             (!file_util::DirectoryExists(dest_path_)) &&
             (IsFileInUse(dest_path_))) {
    // handle overwrite_option_ = NEW_NAME_IF_IN_USE case.
    if (alternative_path_.empty() ||
        file_util::PathExists(alternative_path_) ||
        !file_util::CopyFile(source_path_, alternative_path_)) {
      LOG(ERROR) << "failed to copy " << source_path_.value()
                 << " to " << alternative_path_.value();
      return false;
    } else {
      copied_to_alternate_path_ = true;
      VLOG(1) << "Copied source file " << source_path_.value()
              << " to alternative path " << alternative_path_.value();
      return true;
    }
  } else if ((dest_exist) &&
             (overwrite_option_ == WorkItem::IF_NOT_PRESENT)) {
    // handle overwrite_option_ = IF_NOT_PRESENT case.
    return true;
  }

  // In all cases that reach here, move dest to a backup path.
  if (dest_exist) {
    if (!backup_path_.CreateUniqueTempDirUnderPath(temp_dir_)) {
      PLOG(ERROR) << "Failed to get backup path in folder "
                  << temp_dir_.value();
      return false;
    }

    FilePath backup = backup_path_.path().Append(dest_path_.BaseName());
    if (file_util::Move(dest_path_, backup)) {
      moved_to_backup_ = true;
      VLOG(1) << "Moved destination " << dest_path_.value() <<
                 " to backup path " << backup.value();
    } else {
      LOG(ERROR) << "failed moving " << dest_path_.value()
                 << " to " << backup.value();
      return false;
    }
  }

  // In all cases that reach here, copy source to destination.
  if (file_util::CopyDirectory(source_path_, dest_path_, true)) {
    copied_to_dest_path_ = true;
    VLOG(1) << "Copied source " << source_path_.value()
            << " to destination " << dest_path_.value();
  } else {
    LOG(ERROR) << "failed copy " << source_path_.value()
               << " to " << dest_path_.value();
    return false;
  }

  return true;
}

void CopyTreeWorkItem::Rollback() {
  // Normally the delete operations below should not fail unless some
  // programs like anti-virus are inspecting the files we just copied.
  // If this does happen sometimes, we may consider using Move instead of
  // Delete here. For now we just log the error and continue with the
  // rest of rollback operation.
  if (copied_to_dest_path_ && !file_util::Delete(dest_path_, true)) {
    LOG(ERROR) << "Can not delete " << dest_path_.value();
  }
  if (moved_to_backup_) {
    FilePath backup(backup_path_.path().Append(dest_path_.BaseName()));
    if (!file_util::Move(backup, dest_path_)) {
      LOG(ERROR) << "failed move " << backup.value()
                 << " to " << dest_path_.value();
    }
  }
  if (copied_to_alternate_path_ &&
      !file_util::Delete(alternative_path_, true)) {
    LOG(ERROR) << "Can not delete " << alternative_path_.value();
  }
}

bool CopyTreeWorkItem::IsFileInUse(const FilePath& path) {
  if (!file_util::PathExists(path))
    return false;

  HANDLE handle = ::CreateFile(path.value().c_str(), FILE_ALL_ACCESS,
                               NULL, NULL, OPEN_EXISTING, NULL, NULL);
  if (handle  == INVALID_HANDLE_VALUE)
    return true;

  CloseHandle(handle);
  return false;
}
