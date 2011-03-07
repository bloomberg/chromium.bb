// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_path_watcher/file_path_watcher.h"

#include <CoreServices/CoreServices.h>
#include <set>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/singleton.h"
#include "base/time.h"

namespace {

// The latency parameter passed to FSEventsStreamCreate().
const CFAbsoluteTime kEventLatencySeconds = 0.3;

// Mac-specific file watcher implementation based on the FSEvents API.
class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate {
 public:
  FilePathWatcherImpl();

  // Called from the FSEvents callback whenever there is a change to the paths
  void OnFilePathChanged();

  // (Re-)Initialize the event stream to start reporting events from
  // |start_event|.
  void UpdateEventStream(FSEventStreamEventId start_event);

  // FilePathWatcher::PlatformDelegate overrides.
  virtual bool Watch(const FilePath& path, FilePathWatcher::Delegate* delegate);
  virtual void Cancel();

 private:
  virtual ~FilePathWatcherImpl() {}

  // Destroy the event stream.
  void DestroyEventStream();

  // Delegate to notify upon changes.
  scoped_refptr<FilePathWatcher::Delegate> delegate_;

  // Target path to watch (passed to delegate).
  FilePath target_;

  // Keep track of the last modified time of the file.  We use nulltime
  // to represent the file not existing.
  base::Time last_modified_;

  // The time at which we processed the first notification with the
  // |last_modified_| time stamp.
  base::Time first_notification_;

  // Backend stream we receive event callbacks from (strong reference).
  FSEventStreamRef fsevent_stream_;

  // Used to detect early cancellation.
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(FilePathWatcherImpl);
};

// The callback passed to FSEventStreamCreate().
void FSEventsCallback(ConstFSEventStreamRef stream,
                      void* event_watcher, size_t num_events,
                      void* event_paths, const FSEventStreamEventFlags flags[],
                      const FSEventStreamEventId event_ids[]) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FilePathWatcherImpl* watcher =
      reinterpret_cast<FilePathWatcherImpl*>(event_watcher);
  bool root_changed = false;
  FSEventStreamEventId root_change_at = FSEventStreamGetLatestEventId(stream);
  for (size_t i = 0; i < num_events; i++) {
    if (flags[i] & kFSEventStreamEventFlagRootChanged)
      root_changed = true;
    if (event_ids[i])
      root_change_at = std::min(root_change_at, event_ids[i]);
  }

  // Reinitialize the event stream if we find changes to the root. This is
  // necessary since FSEvents doesn't report any events for the subtree after
  // the directory to be watched gets created.
  if (root_changed) {
    // Resetting the event stream from within the callback fails (FSEvents spews
    // bad file descriptor errors), so post a task to do the reset.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(watcher, &FilePathWatcherImpl::UpdateEventStream,
                          root_change_at));
  }

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(watcher, &FilePathWatcherImpl::OnFilePathChanged));
}

// FilePathWatcherImpl implementation:

FilePathWatcherImpl::FilePathWatcherImpl()
    : fsevent_stream_(NULL),
      canceled_(false) {
}

void FilePathWatcherImpl::OnFilePathChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!target_.empty());

  base::PlatformFileInfo file_info;
  bool file_exists = file_util::GetFileInfo(target_, &file_info);
  if (file_exists && (last_modified_.is_null() ||
      last_modified_ != file_info.last_modified)) {
    last_modified_ = file_info.last_modified;
    first_notification_ = base::Time::Now();
    delegate_->OnFilePathChanged(target_);
  } else if (file_exists && !first_notification_.is_null()) {
    // The target's last modification time is equal to what's on record. This
    // means that either an unrelated event occurred, or the target changed
    // again (file modification times only have a resolution of 1s). Comparing
    // file modification times against the wall clock is not reliable to find
    // out whether the change is recent, since this code might just run too
    // late. Moreover, there's no guarantee that file modification time and wall
    // clock times come from the same source.
    //
    // Instead, the time at which the first notification carrying the current
    // |last_notified_| time stamp is recorded. Later notifications that find
    // the same file modification time only need to be forwarded until wall
    // clock has advanced one second from the initial notification. After that
    // interval, client code is guaranteed to having seen the current revision
    // of the file.
    if (base::Time::Now() - first_notification_ >
        base::TimeDelta::FromSeconds(1)) {
      // Stop further notifications for this |last_modification_| time stamp.
      first_notification_ = base::Time();
    }
    delegate_->OnFilePathChanged(target_);
  } else if (!file_exists && !last_modified_.is_null()) {
    last_modified_ = base::Time();
    delegate_->OnFilePathChanged(target_);
  }
}

bool FilePathWatcherImpl::Watch(const FilePath& path,
                                FilePathWatcher::Delegate* delegate) {
  DCHECK(target_.value().empty());

  target_ = path;
  delegate_ = delegate;

  FSEventStreamEventId start_event = FSEventsGetCurrentEventId();

  base::PlatformFileInfo file_info;
  if (file_util::GetFileInfo(target_, &file_info)) {
    last_modified_ = file_info.last_modified;
    first_notification_ = base::Time::Now();
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &FilePathWatcherImpl::UpdateEventStream,
                        start_event));

  return true;
}

void FilePathWatcherImpl::Cancel() {
  // Switch to the UI thread if necessary, so we can tear down the event stream.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &FilePathWatcherImpl::Cancel));
    return;
  }

  canceled_ = true;
  if (fsevent_stream_)
    DestroyEventStream();
}

void FilePathWatcherImpl::UpdateEventStream(FSEventStreamEventId start_event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // It can happen that the watcher gets canceled while tasks that call this
  // function are still in flight, so abort if this situation is detected.
  if (canceled_)
    return;

  if (fsevent_stream_)
    DestroyEventStream();

  base::mac::ScopedCFTypeRef<CFStringRef> cf_path(CFStringCreateWithCString(
      NULL, target_.value().c_str(), kCFStringEncodingMacHFS));
  base::mac::ScopedCFTypeRef<CFStringRef> cf_dir_path(CFStringCreateWithCString(
      NULL, target_.DirName().value().c_str(), kCFStringEncodingMacHFS));
  CFStringRef paths_array[] = { cf_path.get(), cf_dir_path.get() };
  base::mac::ScopedCFTypeRef<CFArrayRef> watched_paths(CFArrayCreate(
      NULL, reinterpret_cast<const void**>(paths_array), arraysize(paths_array),
      &kCFTypeArrayCallBacks));

  FSEventStreamContext context;
  context.version = 0;
  context.info = this;
  context.retain = NULL;
  context.release = NULL;
  context.copyDescription = NULL;

  fsevent_stream_ = FSEventStreamCreate(NULL, &FSEventsCallback, &context,
                                        watched_paths,
                                        start_event,
                                        kEventLatencySeconds,
                                        kFSEventStreamCreateFlagWatchRoot);
  FSEventStreamScheduleWithRunLoop(fsevent_stream_, CFRunLoopGetCurrent(),
                                   kCFRunLoopDefaultMode);
  if (!FSEventStreamStart(fsevent_stream_)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(delegate_.get(),
                          &FilePathWatcher::Delegate::OnError));
  }
}

void FilePathWatcherImpl::DestroyEventStream() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FSEventStreamStop(fsevent_stream_);
  FSEventStreamUnscheduleFromRunLoop(fsevent_stream_, CFRunLoopGetCurrent(),
                                     kCFRunLoopDefaultMode);
  FSEventStreamRelease(fsevent_stream_);
  fsevent_stream_ = NULL;
}

}  // namespace

FilePathWatcher::FilePathWatcher() {
  impl_ = new FilePathWatcherImpl();
}
