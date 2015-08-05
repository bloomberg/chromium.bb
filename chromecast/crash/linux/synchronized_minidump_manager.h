// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_
#define CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_

#include <time.h>

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chromecast/crash/linux/dump_info.h"

namespace chromecast {

// Abstract base class for mutually-exclusive minidump handling. Ensures
// synchronized access among instances of this class to the minidumps directory
// using a file lock. The "lockfile" also holds serialized metadata about each
// of the minidumps in the directory. Derived classes should not access the
// lockfile directly. Instead, use protected methods to query and modify the
// metadata, but only within the implementation of DoWork().
//
// This class holds an in memory representation of the lockfile while it is
// held. Modifier methods work on this in memory representation. When the
// lockfile is released, the in memory representation is written to file.
class SynchronizedMinidumpManager {
 public:
  // Max number of dumps allowed in lockfile.
  static const int kMaxLockfileDumps;

  // Length of a ratelimit period in seconds.
  static const int kRatelimitPeriodSeconds;

  // Number of dumps allowed per period.
  static const int kRatelimitPeriodMaxDumps;

  virtual ~SynchronizedMinidumpManager();

  // Returns whether this object's file locking method is nonblocking or not.
  bool non_blocking() { return non_blocking_; }

  // Sets the file locking mechansim to be nonblocking or not.
  void set_non_blocking(bool non_blocking) { non_blocking_ = non_blocking; }

 protected:
  SynchronizedMinidumpManager();

  // Acquires the lock, calls DoWork(), then releases the lock when DoWork()
  // returns. Derived classes should expose a method which calls this. Returns
  // the status of DoWork(), or -1 if the lock was not successfully acquired.
  int AcquireLockAndDoWork();

  // Derived classes must implement this method. It will be called from
  // DoWorkLocked after the lock has been successfully acquired. The lockfile
  // shall be accessed and mutated only through the methods below. All other
  // files shall be managed as needed by the derived class.
  virtual int DoWork() = 0;

  // Get the current dumps in the lockfile.
  ScopedVector<DumpInfo> GetDumps();

  // Set |dumps| as the dumps in |lockfile_|, replacing current list of dumps.
  int SetCurrentDumps(const ScopedVector<DumpInfo>& dumps);

  // Serialize |dump_info| and append it to the lockfile. Note that the child
  // class must only call this inside DoWork(). This should be the only method
  // used to write to the lockfile. Only call this if the minidump has been
  // generated in the minidumps directory successfully. Returns 0 on success,
  // -1 otherwise.
  int AddEntryToLockFile(const DumpInfo& dump_info);

  // Remove the lockfile entry at |index| in the current in memory
  // representation of the lockfile. If the index is invalid returns -1.
  // Otherwise returns 0.
  int RemoveEntryFromLockFile(int index);

  // Get the number of un-uploaded dumps in the dump_path directory.
  // If delete_all_dumps is true, also delete all these files, this is used to
  // clean lingering dump files.
  int GetNumDumps(bool delete_all_dumps);

  // If true, the flock on the lockfile will be nonblocking.
  bool non_blocking_;

  // Cached path for the minidumps directory.
  base::FilePath dump_path_;

 private:
  // Acquire the lock file. Blocks if another process holds it, or if called
  // a second time by the same process. Returns the fd of the lockfile if
  // successful, or -1 if failed.
  int AcquireLockFile();

  // Parse the lockfile, populating |lockfile_contents_| for modifier functions
  // to use. Return -1 if an error occurred. Otherwise, return 0. This must not
  // be called unless |this| has acquired the lock.
  int ParseLockFile();

  // Write deserialized lockfile to |lockfile_path_|.
  int WriteLockFile(const base::Value& contents);

  // Creates an empty lock file.
  int CreateEmptyLockFile();

  // Release the lock file with the associated *fd*.
  void ReleaseLockFile();

  // Returns true if |num_dumps| can be written to the lockfile. We can write
  // the dumps if the number of dumps in the lockfile after |num_dumps| is added
  // is less than or equal to |kMaxLockfileDumps| and the number of dumps in the
  // current ratelimit period after |num_dumps| is added is less than or equal
  // to |kRatelimitPeriodMaxDumps|.
  bool CanWriteDumps(int num_dumps);

  std::string lockfile_path_;
  int lockfile_fd_;
  scoped_ptr<base::Value> lockfile_contents_;

  DISALLOW_COPY_AND_ASSIGN(SynchronizedMinidumpManager);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_
