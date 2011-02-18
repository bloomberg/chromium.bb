// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/move_tree_work_item.h"

#include <shlwapi.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/installer/util/logging_installer.h"

MoveTreeWorkItem::~MoveTreeWorkItem() {
}

MoveTreeWorkItem::MoveTreeWorkItem(const FilePath& source_path,
                                   const FilePath& dest_path,
                                   const FilePath& temp_dir)
    : source_path_(source_path),
      dest_path_(dest_path),
      temp_dir_(temp_dir),
      moved_to_dest_path_(false),
      moved_to_backup_(false) {
}

bool MoveTreeWorkItem::Do() {
  if (!file_util::PathExists(source_path_)) {
    LOG(ERROR) << source_path_.value() << " does not exist";
    return false;
  }

  // If dest_path_ exists, move destination to a backup path.
  if (file_util::PathExists(dest_path_)) {
    // Generate a backup path that can keep the original files under dest_path_.
    if (!backup_path_.CreateUniqueTempDirUnderPath(temp_dir_)) {
      PLOG(ERROR) << "Failed to get backup path in folder "
                  << temp_dir_.value();
      return false;
    }

    FilePath backup = backup_path_.path().Append(dest_path_.BaseName());

    if (file_util::Move(dest_path_, backup)) {
      moved_to_backup_ = true;
      VLOG(1) << "Moved destination " << dest_path_.value()
              << " to backup path " << backup.value();
    } else {
      LOG(ERROR) << "failed moving " << dest_path_.value()
                 << " to " << backup.value();
      return false;
    }
  }

  // Now move source to destination.
  if (file_util::Move(source_path_, dest_path_)) {
    moved_to_dest_path_ = true;
    VLOG(1) << "Moved source " << source_path_.value()
            << " to destination " << dest_path_.value();
  } else {
    LOG(ERROR) << "failed move " << source_path_.value()
               << " to " << dest_path_.value();
    return false;
  }

  return true;
}

void MoveTreeWorkItem::Rollback() {
  if (moved_to_dest_path_ && !file_util::Move(dest_path_, source_path_))
    LOG(ERROR) << "Can not move " << dest_path_.value()
               << " to " << source_path_.value();

  if (moved_to_backup_) {
    FilePath backup = backup_path_.path().Append(dest_path_.BaseName());
    if (!file_util::Move(backup, dest_path_)) {
      LOG(ERROR) << "failed move " << backup.value()
                 << " to " << dest_path_.value();
    }
  }
}
