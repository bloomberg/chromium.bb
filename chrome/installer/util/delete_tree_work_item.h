// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
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

  DeleteTreeWorkItem(const base::FilePath& root_path,
                     const base::FilePath& temp_path,
                     const std::vector<base::FilePath>& key_paths);

  // Root path to delete.
  base::FilePath root_path_;

  // Temporary directory that can be used.
  base::FilePath temp_path_;

  // The number of key files.
  ptrdiff_t num_key_files_;

  // Contains the paths to the key files. If specified, deletion will be
  // performed only if none of the key files are in use.
  scoped_ptr<base::FilePath[]> key_paths_;

  // Contains the temp directories for the backed-up key files. The directories
  // are created and populated in Do() as-needed. We don't use a standard
  // container for this since base::ScopedTempDir isn't CopyConstructible.
  scoped_ptr<base::ScopedTempDir[]> key_backup_paths_;

  // The temporary directory into which the original root_path_ has been moved.
  base::ScopedTempDir backup_path_;

  // Set to true once root_path_ has been copied into backup_path_.
  bool copied_to_backup_;
};

#endif  // CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
