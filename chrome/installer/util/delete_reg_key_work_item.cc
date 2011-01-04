// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlwapi.h>

#include "chrome/installer/util/delete_reg_key_work_item.h"

#include "base/logging.h"
#include "chrome/installer/util/logging_installer.h"


DeleteRegKeyWorkItem::~DeleteRegKeyWorkItem() {
}

DeleteRegKeyWorkItem::DeleteRegKeyWorkItem(HKEY predefined_root,
                                           const std::wstring& path)
    : predefined_root_(predefined_root),
      path_(path) {
}

bool DeleteRegKeyWorkItem::Do() {
  // TODO(robertshield): Implement rollback for this work item. Consider using
  // RegSaveKey or RegCopyKey.
  if (!ignore_failure_) {
    NOTREACHED();
    LOG(ERROR) << "Unexpected configuration in DeleteRegKeyWorkItem.";
    return false;
  }

  LSTATUS result = SHDeleteKey(predefined_root_, path_.c_str());
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to delete key at " << path_ << ", result: " << result;
  }

  return true;
}

void DeleteRegKeyWorkItem::Rollback() {
  if (ignore_failure_)
    return;
  NOTREACHED();
}

