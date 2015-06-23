// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/synchronized_minidump_manager.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>

#include "base/logging.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/crash/linux/dump_info.h"

namespace chromecast {

namespace {

const mode_t kDirMode = 0770;
const mode_t kFileMode = 0660;
const char kLockfileName[] = "lockfile";
const char kMinidumpsDir[] = "minidumps";

}  // namespace

SynchronizedMinidumpManager::SynchronizedMinidumpManager()
    : non_blocking_(false), lockfile_fd_(-1) {
  dump_path_ = GetHomePathASCII(kMinidumpsDir);
  lockfile_path_ = dump_path_.Append(kLockfileName).value();
}

SynchronizedMinidumpManager::~SynchronizedMinidumpManager() {
  // Release the lock if held.
  ReleaseLockFile();
}

// TODO(slan): Move some of this pruning logic to ReleaseLockFile?
int SynchronizedMinidumpManager::GetNumDumps(bool delete_all_dumps) {
  DIR* dirp;
  struct dirent* dptr;
  int num_dumps = 0;

  // folder does not exist
  dirp = opendir(dump_path_.value().c_str());
  if (dirp == NULL) {
    LOG(ERROR) << "Unable to open directory " << dump_path_.value();
    return 0;
  }

  while ((dptr = readdir(dirp)) != NULL) {
    struct stat buf;
    const std::string file_fullname = dump_path_.value() + "/" + dptr->d_name;
    if (lstat(file_fullname.c_str(), &buf) == -1 || !S_ISREG(buf.st_mode)) {
      // if we cannot lstat this file, it is probably bad, so skip
      // if the file is not regular, skip
      continue;
    }
    // 'lockfile' is not counted
    if (lockfile_path_ != file_fullname) {
      ++num_dumps;
      if (delete_all_dumps) {
        LOG(INFO) << "Removing " << dptr->d_name
                  << "which was not in the lockfile";
        if (remove(file_fullname.c_str()) < 0) {
          LOG(INFO) << "remove failed. error " << strerror(errno);
        }
      }
    }
  }

  closedir(dirp);
  return num_dumps;
}

int SynchronizedMinidumpManager::AcquireLockAndDoWork() {
  int success = -1;
  if (AcquireLockFile() >= 0) {
    success = DoWork();
    ReleaseLockFile();
  }
  return success;
}

const ScopedVector<DumpInfo>& SynchronizedMinidumpManager::GetDumpMetadata() {
  DCHECK_GE(lockfile_fd_, 0);
  if (!dump_metadata_)
    ParseLockFile();
  return *dump_metadata_;
}

int SynchronizedMinidumpManager::AcquireLockFile() {
  DCHECK_LT(lockfile_fd_, 0);
  // Make the directory for the minidumps if it does not exist.
  if (mkdir(dump_path_.value().c_str(), kDirMode) < 0 && errno != EEXIST) {
    LOG(ERROR) << "mkdir for " << dump_path_.value().c_str()
               << " failed. error = " << strerror(errno);
    return -1;
  }

  // Open the lockfile. Create it if it does not exist.
  lockfile_fd_ = open(lockfile_path_.c_str(), O_RDWR | O_CREAT, kFileMode);

  // If opening or creating the lockfile failed, we don't want to proceed
  // with dump writing for fear of exhausting up system resources.
  if (lockfile_fd_ < 0) {
    LOG(ERROR) << "open lockfile failed " << lockfile_path_;
    return -1;
  }

  // Acquire the lock on the file. Whether or not we are in non-blocking mode,
  // flock failure means that we did not acquire it and this method should fail.
  int operation_mode = non_blocking_ ? (LOCK_EX | LOCK_NB) : LOCK_EX;
  if (flock(lockfile_fd_, operation_mode) < 0) {
    ReleaseLockFile();
    LOG(INFO) << "flock lockfile failed, error = " << strerror(errno);
    return -1;
  }

  // The lockfile is open and locked. Parse it to provide subclasses with a
  // record of all the current dumps.
  if (ParseLockFile() < 0) {
    LOG(ERROR) << "Lockfile did not parse correctly. ";
    return -1;
  }

  // We successfully have acquired the lock.
  return 0;
}

int SynchronizedMinidumpManager::ParseLockFile() {
  DCHECK_GE(lockfile_fd_, 0);
  DCHECK(!dump_metadata_);

  scoped_ptr<ScopedVector<DumpInfo> > dumps(new ScopedVector<DumpInfo>());
  std::string entry;

  // Instead of using |lockfile_fd_|, use <fstream> for readability.
  std::ifstream in(lockfile_path_);
  if (!in.is_open()) {
    NOTREACHED();
    LOG(ERROR) << lockfile_path_ << " could not be opened.";
    return -1;
  }

  // Grab each entry.
  while (std::getline(in, entry)) {
    scoped_ptr<DumpInfo> info(new DumpInfo(entry));
    if (info->valid() && info->crashed_process_dump().size() > 0) {
      dumps->push_back(info.Pass());
    } else {
      LOG(WARNING) << "Entry is not valid: " << entry;
      return -1;
    }
  }

  dump_metadata_ = dumps.Pass();
  return 0;
}

int SynchronizedMinidumpManager::AddEntryToLockFile(const DumpInfo& dump_info) {
  DCHECK_LE(0, lockfile_fd_);

  // Make sure dump_info is valid.
  if (!dump_info.valid()) {
    LOG(ERROR) << "Entry to be added is invalid";
    return -1;
  }

  // Open the file.
  std::ofstream out(lockfile_path_, std::ios::app);
  if (!out.is_open()) {
    NOTREACHED() << "Lockfile would not open.";
    return -1;
  }

  // Write the string and close the file.
  out << dump_info.entry();
  out.close();
  return 0;
}

int SynchronizedMinidumpManager::RemoveEntryFromLockFile(int index) {
  const auto& entries = GetDumpMetadata();
  if (index < 0 || static_cast<size_t>(index) >= entries.size())
    return -1;

  // Remove the entry and write all remaining entries to file.
  dump_metadata_->erase(dump_metadata_->begin() + index);
  std::ofstream out(lockfile_path_);
  for (auto info : *dump_metadata_) {
    out << info->entry();
  }
  out.close();
  return 0;
}

void SynchronizedMinidumpManager::ReleaseLockFile() {
  // flock is associated with the fd entry in the open fd table, so closing
  // all fd's will release the lock. To be safe, we explicitly unlock.
  if (lockfile_fd_ >= 0) {
    flock(lockfile_fd_, LOCK_UN);
    close(lockfile_fd_);

    // We may use this object again, so we should reset this.
    lockfile_fd_ = -1;
  }

  dump_metadata_.reset();
}

}  // namespace chromecast
