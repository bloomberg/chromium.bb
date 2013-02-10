// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_change.h"

namespace drive {

FileChange::FileChange(const base::FilePath& path, Type type)
    : path_(path), type_(type) {
}

FileChange::~FileChange() {}

// static
FileChangeSet FileChange::CreateSingleSet(const base::FilePath& path,
                                          Type type) {
  FileChangeSet result;
  result.insert(FileChange(path, type));
  return result;
}

}  // namespace drive
