// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_
#define CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"

namespace chromecast {

class DumpInfo;

// Abstract base class for mutually-exclusive minidump handling. Ensures
// synchronized access among instances of this class to the minidumps directory
// using a file lock. The "lockfile" also holds serialized metadata about each
// of the minidumps in the directory. Derived classes should not access the
// lockfile directly. Instead, use protected methods to query and modify the
// metadata, but only within the implementation of DoWork().
class SynchronizedMinidumpManager {
 public:
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

  // Access the container holding all the metadata for the dumps. Note that
  // the child class must only call this inside DoWork(). This is lazy. If the
  // lockfile has not been parsed yet, it will be parsed when this is called.
  const ScopedVector<DumpInfo>& GetDumpMetadata();

  // Serialize |dump_info| and append it to the lockfile. Note that the child
  // class must only call this inside DoWork(). This should be the only method
  // used to write to the lockfile. Only call this if the minidump has been
  // generated in the minidumps directory successfully. Returns 0 on success,
  // -1 otherwise.
  int AddEntryToLockFile(const DumpInfo& dump_info);

  // Remove the lockfile entry at |index| in the container returned by
  // GetDumpMetadata(). If the index is invalid or an IO error occurred, returns
  // -1. Otherwise returns 0. When this function returns, both the in-memory
  // containter returned by GetDumpMetadata and the persistent lockfile will be
  // current.
  int RemoveEntryFromLockFile(int index);

  // Get the number of un-uploaded dumps in the dump_path directory.
  // If delete_all_dumps is true, also delete all these files, this is used to
  // clean lingering dump files.
  int GetNumDumps(bool delete_all_dumps);

  // TODO(slan): Remove this accessor. All I/O on the lockfile in inherited
  // classes should be done via GetDumpMetadata(), AddEntryToLockFile(), and
  // RemoveEntryFromLockFile().
  const std::string& lockfile_path() { return lockfile_path_; }

  // If true, the flock on the lockfile will be nonblocking
  bool non_blocking_;

  // Cached path for the minidumps directory.
  base::FilePath dump_path_;

 private:
  // Acquire the lock file. Blocks if another process holds it, or if called
  // a second time by the same process. Returns the fd of the lockfile if
  // successful, or -1 if failed.
  int AcquireLockFile();

  // Parse the lockfile, populating |dumps_| for descendants to use. Return -1
  // if an error occurred. Otherwise, return 0. This must not be called unless
  // |this| has acquired the lock.
  int ParseLockFile();

  // Release the lock file with the associated *fd*.
  void ReleaseLockFile();

  std::string lockfile_path_;
  int lockfile_fd_;
  scoped_ptr<ScopedVector<DumpInfo> > dump_metadata_;

  DISALLOW_COPY_AND_ASSIGN(SynchronizedMinidumpManager);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_
