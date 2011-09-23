// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_COPY_REG_KEY_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_COPY_REG_KEY_WORK_ITEM_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/installer/util/registry_key_backup.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that replaces the contents of one registry key with that
// of another (i.e., the destination is erased prior to the copy).  Be aware
// that in the event of rollback the destination key's values and subkeys are
// restored but the key and its subkeys take on their default security
// descriptors.
class CopyRegKeyWorkItem : public WorkItem {
 public:
  virtual ~CopyRegKeyWorkItem();
  virtual bool Do() OVERRIDE;
  virtual void Rollback() OVERRIDE;

 private:
  friend class WorkItem;

  // Neither |source_key_path| nor |dest_key_path| may be empty.
  // Only ALWAYS and IF_NOT_PRESENT are supported for |overwrite_option|.
  CopyRegKeyWorkItem(HKEY predefined_key,
                     const std::wstring& source_key_path,
                     const std::wstring& dest_key_path,
                     CopyOverWriteOption overwrite_option);

  // Root key in which we operate. The root key must be one of the predefined
  // keys on Windows.
  HKEY predefined_root_;

  // Path of the key to be copied.
  std::wstring source_key_path_;

  // Path of the destination key.
  std::wstring dest_key_path_;

  CopyOverWriteOption overwrite_option_;

  // Backup of the destination key.
  RegistryKeyBackup backup_;

  // Set to true after the destination is cleared for the copy.
  bool cleared_destination_;

  DISALLOW_COPY_AND_ASSIGN(CopyRegKeyWorkItem);
};

#endif  // CHROME_INSTALLER_UTIL_COPY_REG_KEY_WORK_ITEM_H_
