// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_WATCHING_FILE_READER_H_
#define NET_DNS_WATCHING_FILE_READER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"

// Forward declaration
namespace base {
class MessageLoopProxy;
}

namespace net {

// Allows mocking FilePathWatcher
class NET_EXPORT_PRIVATE FilePathWatcherShim
  : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  FilePathWatcherShim();
  virtual ~FilePathWatcherShim();

  virtual bool Watch(
      const FilePath& path,
      base::files::FilePathWatcher::Delegate* delegate) WARN_UNUSED_RESULT;
 private:
  scoped_ptr<base::files::FilePathWatcher> watcher_;
  DISALLOW_COPY_AND_ASSIGN(FilePathWatcherShim);
};

class NET_EXPORT_PRIVATE FilePathWatcherFactory
  : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  FilePathWatcherFactory() {}
  virtual ~FilePathWatcherFactory() {}
  virtual FilePathWatcherShim* CreateFilePathWatcher();
 private:
  DISALLOW_COPY_AND_ASSIGN(FilePathWatcherFactory);
};

// A FilePathWatcher::Delegate that restarts Watch on failures and executes
// DoRead on WorkerPool (one at a time) in response to FilePathChanged.
// Derived classes should store results of the work done in DoRead in dedicated
// fields and read them in OnReadFinished.
//
// This implementation avoids locking by using the reading_ flag to ensure that
// DoRead and OnReadFinished cannot execute in parallel.
//
// There's no need to call Cancel() on destruction. On the contrary, a ref-cycle
// between the WatchingFileReader and a FilePathWatcher will prevent destruction
// until Cancel() is called.
//
// TODO(szym): update to WorkerPool::PostTaskAndReply once available.
class NET_EXPORT_PRIVATE WatchingFileReader
  : public base::files::FilePathWatcher::Delegate {
 public:
  WatchingFileReader();

  // Must be called at most once. Made virtual to allow mocking.
  virtual void StartWatch(const FilePath& path);

  // Unless already scheduled, post DoRead to WorkerPool.
  void ReadNow();

  // Do not perform any further work, except perhaps a latent DoRead().
  // Always call it once done with the delegate to ensure destruction.
  void Cancel();

  void set_watcher_factory(FilePathWatcherFactory* factory) {
    DCHECK(!watcher_.get());
    factory_.reset(factory);
  }

  FilePath get_path() const {
    return path_;
  }

  // Delay between calls to FilePathWatcher::Watch.
  static const int kWatchRetryDelayMs = 100;

  // Delay between calls to WorkerPool::PostTask
  static const int kWorkerPoolRetryDelayMs = 100;

 protected:
  // protected to allow sub-classing, but prevent deleting
  virtual ~WatchingFileReader();

  // Executed on WorkerPool, at most once at a time.
  virtual void DoRead() = 0;
  // Executed on origin thread after DoRead() completes.
  virtual void OnReadFinished() = 0;

 private:
  // FilePathWatcher::Delegate interface
  virtual void OnFilePathChanged(const FilePath& path) OVERRIDE;
  virtual void OnFilePathError(const FilePath& path) OVERRIDE;

  void RescheduleWatch();
  void RestartWatch();

  // Called on the worker thread, executes DoRead and notifies the origin
  // thread.
  void DoReadJob();

  // Called on the the origin thread after DoRead completes.
  void OnReadJobFinished();

  // Message loop for the thread on which watcher is used (of TYPE_IO).
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  FilePath path_;
  scoped_ptr<FilePathWatcherFactory> factory_;
  scoped_ptr<FilePathWatcherShim> watcher_;
  // True after DoRead before OnResultsAvailable.
  bool reading_;
  // True after OnFilePathChanged fires while |reading_| is true.
  bool read_pending_;
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(WatchingFileReader);
};

}  // namespace net

#endif  // NET_DNS_WATCHING_FILE_READER_H_

