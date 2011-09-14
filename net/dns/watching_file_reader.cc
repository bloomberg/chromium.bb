// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/watching_file_reader.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/threading/worker_pool.h"

namespace net {

FilePathWatcherShim::FilePathWatcherShim()
  : watcher_(new base::files::FilePathWatcher()) {}
FilePathWatcherShim::~FilePathWatcherShim() {}

bool FilePathWatcherShim::Watch(
    const FilePath& path,
    base::files::FilePathWatcher::Delegate* delegate) {
  DCHECK(CalledOnValidThread());
  return watcher_->Watch(path, delegate);
}

FilePathWatcherShim*
FilePathWatcherFactory::CreateFilePathWatcher() {
  DCHECK(CalledOnValidThread());
  return new FilePathWatcherShim();
}

WatchingFileReader::WatchingFileReader()
  : message_loop_(base::MessageLoopProxy::current()),
    factory_(new FilePathWatcherFactory()),
    reading_(false),
    read_pending_(false),
    cancelled_(false) {}

void WatchingFileReader::StartWatch(const FilePath& path) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!watcher_.get());
  DCHECK(path_.empty());
  path_ = path;
  RestartWatch();
}

void WatchingFileReader::Cancel() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  cancelled_ = true;
  // Let go of the watcher to break the reference cycle.
  watcher_.reset();
  // Destroy the non-thread-safe factory now, since dtor is non-thread-safe.
  factory_.reset();
}

void WatchingFileReader::OnFilePathChanged(const FilePath& path) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  ReadNow();
}

void WatchingFileReader::OnFilePathError(const FilePath& path) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  RestartWatch();
}

WatchingFileReader::~WatchingFileReader() {
  DCHECK(cancelled_);
}

void WatchingFileReader::RescheduleWatch() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WatchingFileReader::RestartWatch, this),
      kWatchRetryDelayMs);
}

void WatchingFileReader::RestartWatch() {
  if (cancelled_)
    return;
  watcher_.reset(factory_->CreateFilePathWatcher());
  if (watcher_->Watch(path_, this)) {
    ReadNow();
  } else {
    LOG(WARNING) << "Watch on " <<
                    path_.LossyDisplayName() <<
                    " failed, scheduling restart";
    RescheduleWatch();
  }
}

void WatchingFileReader::ReadNow() {
  if (cancelled_)
    return;
  if (reading_) {
    // Remember to re-read after DoRead posts results.
    read_pending_ = true;
  } else {
    if (!base::WorkerPool::PostTask(FROM_HERE, base::Bind(
        &WatchingFileReader::DoReadJob, this), false)) {
#if defined(OS_POSIX)
      // See worker_pool_posix.cc.
      NOTREACHED() << "WorkerPool::PostTask is not expected to fail on posix";
#else
      LOG(WARNING) << "Failed to WorkerPool::PostTask, will retry later";
      message_loop_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&WatchingFileReader::ReadNow, this),
          kWorkerPoolRetryDelayMs);
      return;
#endif
    }
    reading_ = true;
    read_pending_ = false;
  }
}

void WatchingFileReader::DoReadJob() {
  this->DoRead();
  // If this fails, the loop is gone, so there is no point retrying.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &WatchingFileReader::OnReadJobFinished, this));
}

void WatchingFileReader::OnReadJobFinished() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (cancelled_)
    return;
  reading_ = false;
  if (read_pending_) {
    // Discard this result and re-read.
    ReadNow();
    return;
  }
  this->OnReadFinished();
}

}  // namespace net

