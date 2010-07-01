// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_temp_dir.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"

ScopedTempDir::ScopedTempDir() {
}

ScopedTempDir::~ScopedTempDir() {
  if (!path_.empty() && !file_util::Delete(path_, true))
    LOG(ERROR) << "ScopedTempDir unable to delete " << path_.value();
}

bool ScopedTempDir::CreateUniqueTempDir() {
  // This "scoped_dir" prefix is only used on Windows and serves as a template
  // for the unique name.
  if (!file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("scoped_dir"),
                                         &path_))
    return false;

  return true;
}

bool ScopedTempDir::CreateUniqueTempDirUnderPath(const FilePath& base_path,
                                                 bool loose_permissions) {
  // If |path| does not exist, create it.
  if (!file_util::CreateDirectory(base_path)) {
    LOG(ERROR) << "Failed to create base directory " << base_path.value();
    return false;
  }

  // Create a new, uniquely named directory under |base_path|.
  if (!file_util::CreateTemporaryDirInDir(
          base_path,
          FILE_PATH_LITERAL("scoped_dir_"),
          loose_permissions,
          &path_)) {
    return false;
  }
  return true;
}

bool ScopedTempDir::Set(const FilePath& path) {
  DCHECK(path_.empty());
  if (!file_util::DirectoryExists(path) &&
      !file_util::CreateDirectory(path)) {
    return false;
  }
  path_ = path;
  return true;
}

FilePath ScopedTempDir::Take() {
  FilePath ret = path_;
  path_ = FilePath();
  return ret;
}

bool ScopedTempDir::IsValid() const {
  return !path_.empty() && file_util::DirectoryExists(path_);
}
