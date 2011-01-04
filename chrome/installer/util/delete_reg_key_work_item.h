// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_DELETE_REG_KEY_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_DELETE_REG_KEY_WORK_ITEM_H_
#pragma once

#include <windows.h>

#include <string>

#include "base/basictypes.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that deletes a registry key at the given path.
class DeleteRegKeyWorkItem : public WorkItem {
 public:
  virtual ~DeleteRegKeyWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  DeleteRegKeyWorkItem(HKEY predefined_root, const std::wstring& path);

  // Root key from which we delete the key. The root key can only be
  // one of the predefined keys on Windows.
  HKEY predefined_root_;

  // Path of the key to be deleted.
  std::wstring path_;

  // Path in the registry that the key is backed up to.
  std::wstring backup_path_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteRegKeyWorkItem);
};

#endif  // CHROME_INSTALLER_UTIL_DELETE_REG_KEY_WORK_ITEM_H_
