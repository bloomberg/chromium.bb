// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/watching_file_reader.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop.h"
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

// A FilePathWatcher::Delegate that forwards calls to the WatchingFileReader.
// This is separated out to keep WatchingFileReader strictly single-threaded.
class WatchingFileReader::WatcherDelegate
  : public base::files::FilePathWatcher::Delegate {
 public:
  explicit WatcherDelegate(base::WeakPtr<WatchingFileReader> reader)
    : reader_(reader) {}
  void OnFilePathChanged(const FilePath& path) OVERRIDE {
    if (reader_) reader_->OnFilePathChanged(path);
  }
  void OnFilePathError(const FilePath& path) OVERRIDE {
    if (reader_) reader_->OnFilePathError(path);
  }
 private:
  virtual ~WatcherDelegate() {}
  base::WeakPtr<WatchingFileReader> reader_;
};

WatchingFileReader::WatchingFileReader()
  : watcher_factory_(new FilePathWatcherFactory()) {}

WatchingFileReader::~WatchingFileReader() {
  if (reader_)
    reader_->Cancel();
}

void WatchingFileReader::StartWatch(const FilePath& path,
                                    SerialWorker* reader) {
  DCHECK(CalledOnValidThread());
  DCHECK(!watcher_.get());
  DCHECK(!watcher_delegate_.get());
  DCHECK(path_.empty());
  path_ = path;
  watcher_delegate_ = new WatcherDelegate(AsWeakPtr());
  reader_ = reader;
  RestartWatch();
}

void WatchingFileReader::OnFilePathChanged(const FilePath& path) {
  DCHECK(CalledOnValidThread());
  reader_->WorkNow();
}

void WatchingFileReader::OnFilePathError(const FilePath& path) {
  DCHECK(CalledOnValidThread());
  RestartWatch();
}

void WatchingFileReader::RescheduleWatch() {
  DCHECK(CalledOnValidThread());
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WatchingFileReader::RestartWatch, AsWeakPtr()),
      kWatchRetryDelayMs);
}

void WatchingFileReader::RestartWatch() {
  DCHECK(CalledOnValidThread());
  if (reader_->IsCancelled())
    return;
  watcher_.reset(watcher_factory_->CreateFilePathWatcher());
  if (watcher_->Watch(path_, watcher_delegate_)) {
    reader_->WorkNow();
  } else {
    LOG(WARNING) << "Watch on " <<
                    path_.LossyDisplayName() <<
                    " failed, scheduling restart";
    RescheduleWatch();
  }
}

}  // namespace net
