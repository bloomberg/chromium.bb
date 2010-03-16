// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_watcher.h"

#include <CoreServices/CoreServices.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#include "base/time.h"

namespace {

const CFAbsoluteTime kEventLatencySeconds = 0.3;

class FileWatcherImpl : public FileWatcher::PlatformDelegate {
 public:
  FileWatcherImpl() {}
  ~FileWatcherImpl() {
    if (!path_.value().empty()) {
      FSEventStreamStop(fsevent_stream_);
      FSEventStreamInvalidate(fsevent_stream_);
      FSEventStreamRelease(fsevent_stream_);
    }
  }

  virtual bool Watch(const FilePath& path, FileWatcher::Delegate* delegate) {
    FilePath parent_dir = path.DirName();
    if (!file_util::AbsolutePath(&parent_dir))
      return false;

    // Jump back to the UI thread because FSEventStreamScheduleWithRunLoop
    // requires a UI thread.
    if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
      ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
          NewRunnableMethod(this, &FileWatcherImpl::WatchImpl, path, delegate));
    } else {
      LOG(INFO) << "Adding FileWatcher watch.";
      // During unittests, there is only one thread and it is both the UI
      // thread and the file thread.
      WatchImpl(path, delegate);
    }
    return true;
  }

  bool WatchImpl(const FilePath& path, FileWatcher::Delegate* delegate);

  void OnFSEventsCallback(const FilePath& event_path) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
    DCHECK(!path_.value().empty());
    FilePath absolute_event_path = event_path;
    if (!file_util::AbsolutePath(&absolute_event_path))
      return;

    file_util::FileInfo file_info;
    bool file_exists = file_util::GetFileInfo(path_, &file_info);
    if (file_exists && (last_modified_.is_null() ||
        last_modified_ != file_info.last_modified)) {
      last_modified_ = file_info.last_modified;
      delegate_->OnFileChanged(path_);
    } else if (file_exists && (base::Time::Now() - last_modified_ <
               base::TimeDelta::FromSeconds(2))) {
      // Since we only have a resolution of 1s, if we get a callback within
      // 2s of the file having changed, go ahead and notify our observer.  This
      // might be from a different file change, but it's better to notify too
      // much rather than miss a notification.
      delegate_->OnFileChanged(path_);
    } else if (!file_exists && !last_modified_.is_null()) {
      last_modified_ = base::Time();
      delegate_->OnFileChanged(path_);
    }
  }

 private:
  // Delegate to notify upon changes.
  FileWatcher::Delegate* delegate_;

  // Path we're watching (passed to delegate).
  FilePath path_;

  // Backend stream we receive event callbacks from (strong reference).
  FSEventStreamRef fsevent_stream_;

  // Keep track of the last modified time of the file.  We use nulltime
  // to represent the file not existing.
  base::Time last_modified_;

  DISALLOW_COPY_AND_ASSIGN(FileWatcherImpl);
};

void FSEventsCallback(ConstFSEventStreamRef stream,
                      void* event_watcher, size_t num_events,
                      void* event_paths, const FSEventStreamEventFlags flags[],
                      const FSEventStreamEventId event_ids[]) {
  char** paths = reinterpret_cast<char**>(event_paths);
  FileWatcherImpl* watcher =
      reinterpret_cast<FileWatcherImpl*>(event_watcher);
  for (size_t i = 0; i < num_events; i++) {
    if (!ChromeThread::CurrentlyOn(ChromeThread::FILE)) {
      ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
          NewRunnableMethod(watcher, &FileWatcherImpl::OnFSEventsCallback,
                            FilePath(paths[i])));
    } else {
      LOG(INFO) << "FileWatcher event callback for " << paths[i];
      // During unittests, there is only one thread and it is both the UI
      // thread and the file thread.
      watcher->OnFSEventsCallback(FilePath(paths[i]));
    }
  }
}

bool FileWatcherImpl::WatchImpl(const FilePath& path,
                                FileWatcher::Delegate* delegate) {
  DCHECK(path_.value().empty());  // Can only watch one path.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  file_util::FileInfo file_info;
  if (file_util::GetFileInfo(path, &file_info))
    last_modified_ = file_info.last_modified;

  path_ = path;
  delegate_ = delegate;

  scoped_cftyperef<CFStringRef> cf_path(CFStringCreateWithCString(
      NULL, path.DirName().value().c_str(), kCFStringEncodingMacHFS));
  CFStringRef path_for_array = cf_path.get();
  scoped_cftyperef<CFArrayRef> watched_paths(CFArrayCreate(
      NULL, reinterpret_cast<const void**>(&path_for_array), 1,
      &kCFTypeArrayCallBacks));

  FSEventStreamContext context;
  context.version = 0;
  context.info = this;
  context.retain = NULL;
  context.release = NULL;
  context.copyDescription = NULL;

  fsevent_stream_ = FSEventStreamCreate(NULL, &FSEventsCallback, &context,
                                        watched_paths,
                                        kFSEventStreamEventIdSinceNow,
                                        kEventLatencySeconds,
                                        kFSEventStreamCreateFlagNone);
  FSEventStreamScheduleWithRunLoop(fsevent_stream_, CFRunLoopGetCurrent(),
                                   kCFRunLoopDefaultMode);
  FSEventStreamStart(fsevent_stream_);

  return true;
}

}  // namespace

FileWatcher::FileWatcher() {
  impl_ = new FileWatcherImpl();
}
