// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/installer/util/delete_tree_work_item.h"

DeleteTreeWorkItem::DeleteTreeWorkItem(
    const FilePath& root_path,
    const std::vector<FilePath>& key_paths)
    : root_path_(root_path) {
  // Populate our key_paths_ list with empty backup path values.
  std::vector<FilePath>::const_iterator it(key_paths.begin());
  for (; it != key_paths.end(); ++it) {
    key_paths_.push_back(KeyFileList::value_type(*it, FilePath()));
  }
}

DeleteTreeWorkItem::~DeleteTreeWorkItem() {
  if (!backup_path_.empty()) {
    FilePath tmp_dir = backup_path_.DirName();
    if (file_util::PathExists(tmp_dir)) {
      file_util::Delete(tmp_dir, true);
    }
  }

  KeyFileList::const_iterator it(key_paths_.begin());
  for (; it != key_paths_.end(); ++it) {
    if (!it->second.empty()) {
      FilePath tmp_dir = it->second.DirName();
      if (file_util::PathExists(tmp_dir)) {
        file_util::Delete(tmp_dir, true);
      }
    }
  }
}

// We first try to move key_path_ to backup_path. If it succeeds, we go ahead
// and move the rest.
bool DeleteTreeWorkItem::Do() {
  // Go through all the key files and see if we can open them exclusively
  // with only the FILE_SHARE_DELETE flag.  Once we know we have all of them,
  // we can delete them.
  KeyFileList::iterator it(key_paths_.begin());
  std::vector<HANDLE> opened_key_files;
  bool abort = false;
  for (; !abort && it != key_paths_.end(); ++it) {
    if (!GetBackupPath(it->first, &it->second) ||
        !file_util::CopyFile(it->first, it->second)) {
      PLOG(ERROR) << "Could not back up: " << it->first.value();
      abort = true;
    } else {
      HANDLE file = ::CreateFile(it->first.value().c_str(), FILE_ALL_ACCESS,
                                 FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0,
                                 NULL);
      if (file != INVALID_HANDLE_VALUE) {
        VLOG(1) << "Acquired exclusive lock for key file: "
                << it->first.value();
        opened_key_files.push_back(file);
      } else {
        if (::GetLastError() != ERROR_FILE_NOT_FOUND)
          abort = true;
        PLOG(INFO) << "Failed to open " << it->first.value();
      }
    }
  }

  if (!abort) {
    // We now hold exclusive locks with "share delete" permissions for each
    // of the key files and also have created backups of those files.
    // We can safely delete the key files now.
    it = key_paths_.begin();
    for (; !abort && it != key_paths_.end(); ++it) {
      if (!file_util::Delete(it->first, true)) {
        // This should not really be possible because of the above.
        NOTREACHED();
        PLOG(ERROR) << "Unexpectedly could not delete " << it->first.value();
        abort = true;
      }
    }
  }

  std::vector<HANDLE>::const_iterator file_it(opened_key_files.begin());
  for (; file_it != opened_key_files.end(); ++file_it)
    ::CloseHandle(*file_it);

  opened_key_files.clear();

  if (abort) {
    LOG(ERROR) << "Could not exclusively hold all key files.";
    return false;
  }

  // Now that we've taken care of the key files, take care of the rest.
  if (!root_path_.empty() && file_util::PathExists(root_path_)) {
    if (!GetBackupPath(root_path_, &backup_path_) ||
        !file_util::CopyDirectory(root_path_, backup_path_, true) ||
        !file_util::Delete(root_path_, true)) {
      LOG(ERROR) << "can not delete " << root_path_.value()
                 << " OR copy it to backup path " << backup_path_.value();
      return false;
    }
  }

  return true;
}

// If there are files in backup paths move them back.
void DeleteTreeWorkItem::Rollback() {
  if (!backup_path_.empty() && file_util::PathExists(backup_path_)) {
    file_util::Move(backup_path_, root_path_);
  }

  KeyFileList::const_iterator it(key_paths_.begin());
  for (; it != key_paths_.end(); ++it) {
    if (!it->second.empty() && file_util::PathExists(it->second)) {
      if (!file_util::Move(it->second, it->first)) {
        // This could happen if we could not delete the key file to begin with.
        PLOG(WARNING) << "Rollback: Failed to move backup file back in place: "
                      << it->second.value() << " to " << it->first.value();
      }
    }
  }
}

bool DeleteTreeWorkItem::GetBackupPath(const FilePath& for_path,
                                       FilePath* backup_path) {
  if (!file_util::CreateNewTempDirectory(L"", backup_path)) {
    // We assume that CreateNewTempDirectory() is doing its job well.
    LOG(ERROR) << "Couldn't get backup path for delete.";
    return false;
  }

  *backup_path = backup_path->Append(for_path.BaseName());
  return true;
}
