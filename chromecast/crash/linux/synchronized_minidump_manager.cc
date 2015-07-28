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
#include "chromecast/base/serializers.h"
#include "chromecast/crash/linux/dump_info.h"

namespace chromecast {

namespace {

const mode_t kDirMode = 0770;
const mode_t kFileMode = 0660;
const char kLockfileName[] = "lockfile";
const char kMinidumpsDir[] = "minidumps";

const char kLockfileDumpField[] = "dumps";

// Gets the list of deserialized DumpInfo given a deserialized |lockfile|.
base::ListValue* GetDumpList(base::Value* lockfile) {
  base::DictionaryValue* dict;
  base::ListValue* dump_list;
  if (!lockfile || !lockfile->GetAsDictionary(&dict) ||
      !dict->GetList(kLockfileDumpField, &dump_list)) {
    return nullptr;
  }

  return dump_list;
}

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
    if (CreateEmptyLockFile() < 0 || ParseLockFile() < 0) {
      LOG(ERROR) << "Failed to create a new lock file!";
      return -1;
    }
  }

  DCHECK(lockfile_contents_);
  // We successfully have acquired the lock.
  return 0;
}

int SynchronizedMinidumpManager::ParseLockFile() {
  DCHECK_GE(lockfile_fd_, 0);
  DCHECK(!lockfile_contents_);

  base::FilePath lockfile_path(lockfile_path_);
  lockfile_contents_ = DeserializeJsonFromFile(lockfile_path);

  return lockfile_contents_ ? 0 : -1;
}

int SynchronizedMinidumpManager::WriteLockFile(const base::Value& contents) {
  base::FilePath lockfile_path(lockfile_path_);
  return SerializeJsonToFile(lockfile_path, contents) ? 0 : -1;
}

int SynchronizedMinidumpManager::CreateEmptyLockFile() {
  scoped_ptr<base::DictionaryValue> output =
      make_scoped_ptr(new base::DictionaryValue());
  output->Set(kLockfileDumpField, make_scoped_ptr(new base::ListValue()));

  return WriteLockFile(*output);
}

int SynchronizedMinidumpManager::AddEntryToLockFile(const DumpInfo& dump_info) {
  DCHECK_LE(0, lockfile_fd_);

  // Make sure dump_info is valid.
  if (!dump_info.valid()) {
    LOG(ERROR) << "Entry to be added is invalid";
    return -1;
  }

  base::ListValue* entry_list = GetDumpList(lockfile_contents_.get());
  if (!entry_list) {
    LOG(ERROR) << "Failed to parse lock file";
    return -1;
  }

  entry_list->Append(dump_info.GetAsValue());

  return 0;
}

int SynchronizedMinidumpManager::RemoveEntryFromLockFile(int index) {
  base::ListValue* entry_list = GetDumpList(lockfile_contents_.get());
  if (!entry_list) {
    LOG(ERROR) << "Failed to parse lock file";
    return -1;
  }

  if (!entry_list->Remove(static_cast<size_t>(index), nullptr)) {
    return -1;
  }

  return 0;
}

void SynchronizedMinidumpManager::ReleaseLockFile() {
  // flock is associated with the fd entry in the open fd table, so closing
  // all fd's will release the lock. To be safe, we explicitly unlock.
  if (lockfile_fd_ >= 0) {
    if (lockfile_contents_) {
      WriteLockFile(*lockfile_contents_);
    }
    flock(lockfile_fd_, LOCK_UN);
    close(lockfile_fd_);

    // We may use this object again, so we should reset this.
    lockfile_fd_ = -1;
  }

  lockfile_contents_.reset();
}

ScopedVector<DumpInfo> SynchronizedMinidumpManager::GetDumps() {
  ScopedVector<DumpInfo> dumps;
  const base::ListValue* dump_list = GetDumpList(lockfile_contents_.get());

  if (dump_list == nullptr) {
    return dumps.Pass();
  }

  for (const base::Value* elem : *dump_list) {
    dumps.push_back(new DumpInfo(elem));
  }

  return dumps.Pass();
}

int SynchronizedMinidumpManager::SetCurrentDumps(
    const ScopedVector<DumpInfo>& dumps) {
  base::ListValue* dump_list = GetDumpList(lockfile_contents_.get());
  if (dump_list == nullptr) {
    LOG(ERROR) << "Invalid lock file";
    return -1;
  }

  dump_list->Clear();

  for (DumpInfo* dump : dumps) {
    dump_list->Append(dump->GetAsValue());
  }

  return 0;
}

}  // namespace chromecast
