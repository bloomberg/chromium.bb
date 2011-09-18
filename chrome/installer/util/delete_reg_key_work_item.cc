// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_reg_key_work_item.h"

#include <shlwapi.h>

#include "base/logging.h"
#include "base/win/registry.h"

using base::win::RegKey;

DeleteRegKeyWorkItem::~DeleteRegKeyWorkItem() {
}

DeleteRegKeyWorkItem::DeleteRegKeyWorkItem(HKEY predefined_root,
                                           const std::wstring& path)
    : predefined_root_(predefined_root),
      path_(path) {
  DCHECK(predefined_root);
  // It's a safe bet that we don't want to delete one of the root trees.
  DCHECK(!path.empty());
}

bool DeleteRegKeyWorkItem::Do() {
  if (path_.empty())
    return false;

  RegistryKeyBackup backup;

  // Only try to make a backup if we're not configured to ignore failures.
  if (!ignore_failure_) {
    if (!backup.Initialize(predefined_root_, path_.c_str())) {
      LOG(ERROR) << "Failed to backup destination for registry key copy.";
      return false;
    }
  }

  // Delete the key.
  LONG result = SHDeleteKey(predefined_root_, path_.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete key at " << path_ << ", result: "
               << result;
    return ignore_failure_;
  }

  // We've succeeded, so remember any backup we may have made.
  backup_.swap(backup);

  return true;
}

void DeleteRegKeyWorkItem::Rollback() {
  if (ignore_failure_)
    return;

  // Delete anything in the key before restoring the backup in case someone else
  // put new data in the key after Do().
  LONG result = SHDeleteKey(predefined_root_, path_.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete key at " << path_ << " in rollback, "
                  "result: " << result;
  }

  // Restore the old contents.  The restoration takes on its default security
  // attributes; any custom attributes are lost.
  if (!backup_.WriteTo(predefined_root_, path_.c_str()))
    LOG(ERROR) << "Failed to restore key in rollback.";
}
