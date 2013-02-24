// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CHANGE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CHANGE_H_

#include <set>

#include "base/files/file_path.h"

namespace drive {

class FileChange;

// Set of changes.
typedef std::set<FileChange> FileChangeSet;

// Represents change in the filesystem. Rename is represented as two entries:
// of type DELETED and ADDED. CHANGED type is for changed contents of
// directories or for changed metadata and/or contents of files.
class FileChange {
 public:
  enum Type {
    DELETED,
    ADDED,
    CHANGED,
  };

  // Created an object representing a change of file or directory pointed by
  // |change_path|. The change is of |change_type| type.
  FileChange(const base::FilePath& path, Type type);
  ~FileChange();

  // Factory method to create a FileChangeSet object with only one element.
  static FileChangeSet CreateSingleSet(const base::FilePath& path, Type type);

  bool operator==(const FileChange &file_change) const {
    return path_ == file_change.path() && type_ == file_change.type();
  }

  bool operator<(const FileChange &file_change) const {
    return (path_ < file_change.path()) ||
           (path_ == file_change.path() && type_ < file_change.type());
  }

  const base::FilePath& path() const { return path_; }

  Type type() const { return type_; }

 private:
  const base::FilePath path_;
  const Type type_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CHANGE_H_
