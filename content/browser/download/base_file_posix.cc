// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/base_file.h"

#include "base/files/file_util.h"
#include "content/public/browser/download_interrupt_reasons.h"

namespace content {

DownloadInterruptReason BaseFile::MoveFileAndAdjustPermissions(
    const base::FilePath& new_path) {
  // Move |full_path_|, created with mode 0600, to |new_path|.
  //
  // If |new_path| does not already exist, create it. The kernel will apply
  // the user's umask to the mode 0666.
  if (!base::PathExists(new_path)) {
    if (!base::WriteFileWithMode(new_path, "", 0, 0666))
      return LogSystemError("WriteFile", errno);
  }

  // Whether newly-created or pre-existing, get the mode of the file named
  // |new_path|.
  mode_t mode;
  struct stat status;
  if (stat(new_path.value().c_str(), &status)) {
    return LogSystemError("WriteFile", errno);
  }
  mode = status.st_mode & 0777;

  // If rename(2) fails, fall back to base::Move.
  if (rename(full_path_.value().c_str(), new_path.value().c_str())) {
    if (!base::Move(full_path_, new_path))
      return LogSystemError("Move", errno);
  }

  // If |base::Move| had to copy the file (e.g. because the source is on a
  // different volume than |new_path|, we must re-set the mode. This is
  // racy but may be the best we can do.
  //
  // On Windows file systems (FAT, NTFS), chmod fails. This is OK.
  if (chmod(new_path.value().c_str(), mode))
    (void) LogSystemError("chmod", errno);

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

}  // namespace content
