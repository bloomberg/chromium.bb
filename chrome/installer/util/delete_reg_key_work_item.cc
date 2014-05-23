// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_reg_key_work_item.h"

#include <shlwapi.h>

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/install_util.h"

using base::win::RegKey;

DeleteRegKeyWorkItem::~DeleteRegKeyWorkItem() {
}

DeleteRegKeyWorkItem::DeleteRegKeyWorkItem(HKEY predefined_root,
                                           const std::wstring& path,
                                           REGSAM wow64_access)
    : predefined_root_(predefined_root),
      path_(path),
      wow64_access_(wow64_access) {
  DCHECK(predefined_root);
  // It's a safe bet that we don't want to delete one of the root trees.
  DCHECK(!path.empty());
  DCHECK(wow64_access == 0 ||
         wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);
}

bool DeleteRegKeyWorkItem::Do() {
  if (path_.empty())
    return false;

  RegistryKeyBackup backup;

  // Only try to make a backup if we're not configured to ignore failures.
  if (!ignore_failure_) {
    if (!backup.Initialize(predefined_root_, path_.c_str(), wow64_access_)) {
      LOG(ERROR) << "Failed to backup destination for registry key copy.";
      return false;
    }
  }

  // Delete the key.
  if (!InstallUtil::DeleteRegistryKey(
          predefined_root_, path_.c_str(), wow64_access_)) {
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
  InstallUtil::DeleteRegistryKey(predefined_root_,
                                 path_.c_str(),
                                 wow64_access_);

  // Restore the old contents.  The restoration takes on its default security
  // attributes; any custom attributes are lost.
  if (!backup_.WriteTo(predefined_root_, path_.c_str(), wow64_access_))
    LOG(ERROR) << "Failed to restore key in rollback.";
}
