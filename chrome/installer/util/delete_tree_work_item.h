// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively deletes a file system hierarchy at the
// given root path. The file system hierarchy could be a single file, or a
// directory.
// The file system hierarchy to be deleted can have one or more key files. If
// specified, deletion will be performed only if all key files are not in use.
class DeleteTreeWorkItem : public WorkItem {
 public:
  virtual ~DeleteTreeWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  DeleteTreeWorkItem(const FilePath& root_path,
                     const FilePath& temp_path,
                     const std::vector<FilePath>& key_paths);

  // Root path to delete.
  FilePath root_path_;

  // Temporary directory that can be used.
  FilePath temp_path_;

  // The number of key files.
  ptrdiff_t num_key_files_;

  // Contains the paths to the key files. If specified, deletion will be
  // performed only if none of the key files are in use.
  scoped_array<FilePath> key_paths_;

  // Contains the temp directories for the backed-up key files. The directories
  // are created and populated in Do() as-needed. We don't use a standard
  // container for this since ScopedTempDir isn't CopyConstructible.
  scoped_array<ScopedTempDir> key_backup_paths_;

  // The temporary directory into which the original root_path_ has been moved.
  ScopedTempDir backup_path_;
};

#endif  // CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
