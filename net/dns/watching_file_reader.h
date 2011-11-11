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
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"
#include "net/dns/serial_worker.h"

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

// Convenience wrapper which maintains a FilePathWatcher::Delegate, restarts
// Watch on failures and calls PooledReader::ReadNow when OnFilePathChanged.
// Cancels the PooledReader upon destruction.
class NET_EXPORT_PRIVATE WatchingFileReader
  : NON_EXPORTED_BASE(public base::NonThreadSafe),
    NON_EXPORTED_BASE(public base::SupportsWeakPtr<WatchingFileReader>) {
 public:
  WatchingFileReader();
  virtual ~WatchingFileReader();

  // Must be called at most once. Made virtual to allow mocking.
  virtual void StartWatch(const FilePath& path, SerialWorker* reader);

  void set_watcher_factory(FilePathWatcherFactory* factory) {
    DCHECK(!watcher_.get());
    watcher_factory_.reset(factory);
  }

  FilePath get_path() const {
    return path_;
  }

  // Delay between calls to FilePathWatcher::Watch.
  static const int kWatchRetryDelayMs = 100;

 private:
  class WatcherDelegate;
  // FilePathWatcher::Delegate interface exported via WatcherDelegate
  virtual void OnFilePathChanged(const FilePath& path);
  virtual void OnFilePathError(const FilePath& path);

  void RescheduleWatch();
  void RestartWatch();

  FilePath path_;
  scoped_ptr<FilePathWatcherFactory> watcher_factory_;
  scoped_ptr<FilePathWatcherShim> watcher_;
  scoped_refptr<WatcherDelegate> watcher_delegate_;
  scoped_refptr<SerialWorker> reader_;

  DISALLOW_COPY_AND_ASSIGN(WatchingFileReader);
};

}  // namespace net

#endif  // NET_DNS_WATCHING_FILE_READER_H_
