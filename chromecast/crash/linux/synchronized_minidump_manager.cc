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
#include <time.h>
#include <unistd.h>

#include "base/logging.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/serializers.h"
#include "chromecast/crash/linux/dump_info.h"

// if |cond| is false, returns |retval|.
#define RCHECK(cond, retval) \
  do {                       \
    if (!(cond)) {           \
      return (retval);       \
    }                        \
  } while (0)

namespace chromecast {

namespace {

const mode_t kDirMode = 0770;
const mode_t kFileMode = 0660;
const char kLockfileName[] = "lockfile";
const char kMinidumpsDir[] = "minidumps";

const char kLockfileDumpKey[] = "dumps";
const char kLockfileRatelimitKey[] = "ratelimit";
const char kLockfileRatelimitPeriodStartKey[] = "period_start";
const char kLockfileRatelimitPeriodDumpsKey[] = "period_dumps";

// Gets the list of deserialized DumpInfo given a deserialized |lockfile|.
// Returns nullptr if invalid.
base::ListValue* GetDumpList(base::Value* lockfile) {
  base::DictionaryValue* dict;
  base::ListValue* dump_list;
  if (!lockfile || !lockfile->GetAsDictionary(&dict) ||
      !dict->GetList(kLockfileDumpKey, &dump_list)) {
    return nullptr;
  }

  return dump_list;
}

// Gets the ratelimit parameter dictionary given a deserialized |lockfile|.
// Returns nullptr if invalid.
base::DictionaryValue* GetRatelimitParams(base::Value* lockfile) {
  base::DictionaryValue* dict;
  base::DictionaryValue* ratelimit_params;
  if (!lockfile || !lockfile->GetAsDictionary(&dict) ||
      !dict->GetDictionary(kLockfileRatelimitKey, &ratelimit_params)) {
    return nullptr;
  }

  return ratelimit_params;
}

// Returns the time of the current ratelimit period's start in |lockfile|.
// Returns (time_t)-1 if an error occurs.
time_t GetRatelimitPeriodStart(base::Value* lockfile) {
  time_t result = static_cast<time_t>(-1);
  base::DictionaryValue* ratelimit_params = GetRatelimitParams(lockfile);
  RCHECK(ratelimit_params, result);

  std::string period_start_str;
  RCHECK(ratelimit_params->GetString(kLockfileRatelimitPeriodStartKey,
                                     &period_start_str),
         result);

  errno = 0;
  result = static_cast<time_t>(strtoll(period_start_str.c_str(), nullptr, 0));
  if (errno != 0) {
    return static_cast<time_t>(-1);
  }

  return result;
}

// Sets the time of the current ratelimit period's start in |lockfile| to
// |period_start|. Returns 0 on success, < 0 on error.
int SetRatelimitPeriodStart(base::Value* lockfile, time_t period_start) {
  DCHECK_GE(period_start, 0);

  base::DictionaryValue* ratelimit_params = GetRatelimitParams(lockfile);
  RCHECK(ratelimit_params, -1);

  char buf[128];
  snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(period_start));
  std::string period_start_str(buf);
  ratelimit_params->SetString(kLockfileRatelimitPeriodStartKey,
                              period_start_str);
  return 0;
}

// Gets the number of dumps added to |lockfile| in the current ratelimit
// period. Returns < 0 on error.
int GetRatelimitPeriodDumps(base::Value* lockfile) {
  int period_dumps = -1;

  base::DictionaryValue* ratelimit_params = GetRatelimitParams(lockfile);
  if (!ratelimit_params ||
      !ratelimit_params->GetInteger(kLockfileRatelimitPeriodDumpsKey,
                                    &period_dumps)) {
    return -1;
  }

  return period_dumps;
}

// Sets the current ratelimit period's number of dumps in |lockfile| to
// |period_dumps|. Returns 0 on success, < 0 on error.
int SetRatelimitPeriodDumps(base::Value* lockfile, int period_dumps) {
  DCHECK_GE(period_dumps, 0);

  base::DictionaryValue* ratelimit_params = GetRatelimitParams(lockfile);
  RCHECK(ratelimit_params, -1);

  ratelimit_params->SetInteger(kLockfileRatelimitPeriodDumpsKey, period_dumps);

  return 0;
}

// Increment the number of dumps in the current ratelimit period in deserialized
// |lockfile| by |increment|. Returns 0 on success, < 0 on error.
int IncrementCurrentPeriodDumps(base::Value* lockfile, int increment) {
  DCHECK_GE(increment, 0);
  int last_dumps = GetRatelimitPeriodDumps(lockfile);
  RCHECK(last_dumps >= 0, -1);

  return SetRatelimitPeriodDumps(lockfile, last_dumps + increment);
}

}  // namespace

const int SynchronizedMinidumpManager::kMaxLockfileDumps = 5;

// One day
const int SynchronizedMinidumpManager::kRatelimitPeriodSeconds = 24 * 3600;
const int SynchronizedMinidumpManager::kRatelimitPeriodMaxDumps = 100;

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

  // Verify validity of lockfile
  if (!GetDumpList(lockfile_contents_.get()) ||
      GetRatelimitPeriodStart(lockfile_contents_.get()) < 0 ||
      GetRatelimitPeriodDumps(lockfile_contents_.get()) < 0) {
    lockfile_contents_ = nullptr;
    return -1;
  }

  return 0;
}

int SynchronizedMinidumpManager::WriteLockFile(const base::Value& contents) {
  base::FilePath lockfile_path(lockfile_path_);
  return SerializeJsonToFile(lockfile_path, contents) ? 0 : -1;
}

int SynchronizedMinidumpManager::CreateEmptyLockFile() {
  scoped_ptr<base::DictionaryValue> output(
      make_scoped_ptr(new base::DictionaryValue()));
  output->Set(kLockfileDumpKey, make_scoped_ptr(new base::ListValue()));

  base::DictionaryValue* ratelimit_fields = new base::DictionaryValue();
  output->Set(kLockfileRatelimitKey, make_scoped_ptr(ratelimit_fields));
  ratelimit_fields->SetString(kLockfileRatelimitPeriodStartKey, "0");
  ratelimit_fields->SetInteger(kLockfileRatelimitPeriodDumpsKey, 0);

  return WriteLockFile(*output);
}

int SynchronizedMinidumpManager::AddEntryToLockFile(const DumpInfo& dump_info) {
  DCHECK_LE(0, lockfile_fd_);

  // Make sure dump_info is valid.
  if (!dump_info.valid()) {
    LOG(ERROR) << "Entry to be added is invalid";
    return -1;
  }

  if (!CanWriteDumps(1)) {
    LOG(ERROR) << "Can't Add Dump: Ratelimited";
    return -1;
  }

  base::ListValue* entry_list = GetDumpList(lockfile_contents_.get());
  if (!entry_list) {
    LOG(ERROR) << "Failed to parse lock file";
    return -1;
  }

  IncrementCurrentPeriodDumps(lockfile_contents_.get(), 1);

  entry_list->Append(dump_info.GetAsValue());

  return 0;
}

int SynchronizedMinidumpManager::RemoveEntryFromLockFile(int index) {
  base::ListValue* entry_list = GetDumpList(lockfile_contents_.get());
  if (!entry_list) {
    LOG(ERROR) << "Failed to parse lock file";
    return -1;
  }

  RCHECK(entry_list->Remove(static_cast<size_t>(index), nullptr), -1);
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
  RCHECK(dump_list, dumps.Pass());

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

bool SynchronizedMinidumpManager::CanWriteDumps(int num_dumps) {
  const auto dumps(GetDumps());

  // If no more dumps can be written, return false.
  if (static_cast<int>(dumps.size()) + num_dumps > kMaxLockfileDumps)
    return false;

  // If too many dumps have been written recently, return false.
  time_t cur_time = time(nullptr);
  time_t period_start = GetRatelimitPeriodStart(lockfile_contents_.get());
  int period_dumps_count = GetRatelimitPeriodDumps(lockfile_contents_.get());

  // If we're in invalid state, or we passed the period, reset the ratelimit.
  // When the device reboots, |cur_time| may be incorrectly reported to be a
  // very small number for a short period of time. So only consider
  // |period_start| invalid when |cur_time| is less if |cur_time| is not very
  // close to 0.
  if (period_dumps_count < 0 ||
      (cur_time < period_start && cur_time > kRatelimitPeriodSeconds) ||
      difftime(cur_time, period_start) >= kRatelimitPeriodSeconds) {
    period_start = cur_time;
    period_dumps_count = 0;
    SetRatelimitPeriodStart(lockfile_contents_.get(), period_start);
    SetRatelimitPeriodDumps(lockfile_contents_.get(), period_dumps_count);
  }

  return period_dumps_count + num_dumps <= kRatelimitPeriodMaxDumps;
}

}  // namespace chromecast
