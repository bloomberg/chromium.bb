// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/task_runner.h"
#include "base/time.h"
#include "net/base/completion_callback.h"
#include "net/disk_cache/simple/simple_disk_format.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class IOBuffer;
}

namespace disk_cache {

// Worker thread interface to the very simple cache. This interface is not
// thread safe, and callers must insure that it is only ever accessed from
// a single thread between synchronization points.
class SimpleSynchronousEntry {
 public:
  typedef base::Callback<void(SimpleSynchronousEntry*)>
      SynchronousCreationCallback;

  typedef base::Callback<void(int)>
      SynchronousOperationCallback;

  static void OpenEntry(
      const base::FilePath& path,
      const std::string& key,
      const scoped_refptr<base::TaskRunner>& callback_runner,
      const SynchronousCreationCallback& callback);

  static void CreateEntry(
      const base::FilePath& path,
      const std::string& key,
      const scoped_refptr<base::TaskRunner>& callback_runner,
      const SynchronousCreationCallback& callback);

  // Deletes an entry without first Opening it. Does not check if there is
  // already an Entry object in memory holding the open files. Be careful! This
  // is meant to be used by the Backend::DoomEntry() call. |callback| will be
  // run by |callback_runner|.
  static void DoomEntry(const base::FilePath& path,
                        const std::string& key,
                        scoped_refptr<base::TaskRunner> callback_runner,
                        const net::CompletionCallback& callback);

  // N.B. DoomAndClose(), Close(), ReadData() and WriteData() may block on IO.
  void DoomAndClose();
  void Close();
  void ReadData(int index,
                int offset,
                net::IOBuffer* buf,
                int buf_len,
                const SynchronousOperationCallback& callback);
  void WriteData(int index,
                 int offset,
                 net::IOBuffer* buf,
                 int buf_len,
                 const SynchronousOperationCallback& callback,
                 bool truncate);

  const base::FilePath& path() const { return path_; }
  std::string key() const { return key_; }
  base::Time last_used() const { return last_used_; }
  base::Time last_modified() const { return last_modified_; }
  int32 data_size(int index) const { return data_size_[index]; }

 private:
  SimpleSynchronousEntry(
      const scoped_refptr<base::TaskRunner>& callback_runner,
      const base::FilePath& path,
      const std::string& key);

  // Like Entry, the SimpleSynchronousEntry self releases when Close() is
  // called.
  ~SimpleSynchronousEntry();

  bool OpenOrCreateFiles(bool create);
  bool InitializeForOpen();
  bool InitializeForCreate();

  scoped_refptr<base::TaskRunner> callback_runner_;
  const base::FilePath path_;
  const std::string key_;

  bool initialized_;

  base::Time last_used_;
  base::Time last_modified_;
  int32 data_size_[kSimpleEntryFileCount];

  base::PlatformFile files_[kSimpleEntryFileCount];
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_
