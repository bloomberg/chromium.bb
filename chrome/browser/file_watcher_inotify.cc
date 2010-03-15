// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_watcher.h"

#include <errno.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/waitable_event.h"

namespace {

class FileWatcherImpl;

// Singleton to manage all inotify watches.
// TODO(tony): It would be nice if this wasn't a singleton.
// http://crbug.com/38174
class InotifyReader {
 public:
  typedef int Watch;  // Watch descriptor used by AddWatch and RemoveWatch.
  static const Watch kInvalidWatch = -1;

  // Watch |path| for changes. |watcher| will be notified on each change.
  // Returns kInvalidWatch on failure.
  Watch AddWatch(const FilePath& path, FileWatcherImpl* watcher);

  // Remove |watch|. Returns true on success.
  bool RemoveWatch(Watch watch, FileWatcherImpl* watcher);

  // Callback for InotifyReaderTask.
  void OnInotifyEvent(const inotify_event* event);

 private:
  friend struct DefaultSingletonTraits<InotifyReader>;

  typedef std::set<FileWatcherImpl*> WatcherSet;

  InotifyReader();
  ~InotifyReader();

  // We keep track of which delegates want to be notified on which watches.
  base::hash_map<Watch, WatcherSet> watchers_;

  // Lock to protect watchers_.
  Lock lock_;

  // Separate thread on which we run blocking read for inotify events.
  base::Thread thread_;

  // File descriptor returned by inotify_init.
  const int inotify_fd_;

  // Use self-pipe trick to unblock select during shutdown.
  int shutdown_pipe_[2];

  // Flag set to true when startup was successful.
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(InotifyReader);
};

class FileWatcherImpl : public FileWatcher::PlatformDelegate {
 public:
  FileWatcherImpl();
  ~FileWatcherImpl();

  // Called for each event coming from the watch.
  void OnInotifyEvent(const inotify_event* event);

  // Start watching |path| for changes and notify |delegate| on each change.
  // Returns true if watch for |path| has been added successfully.
  virtual bool Watch(const FilePath& path, FileWatcher::Delegate* delegate);

 private:
  // Delegate to notify upon changes.
  FileWatcher::Delegate* delegate_;

  // Watch returned by InotifyReader.
  InotifyReader::Watch watch_;

  // The file we're watching.
  FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(FileWatcherImpl);
};

class FileWatcherImplNotifyTask : public Task {
 public:
  FileWatcherImplNotifyTask(FileWatcher::Delegate* delegate,
                            const FilePath& path)
      : delegate_(delegate), path_(path) {
  }

  virtual void Run() {
    delegate_->OnFileChanged(path_);
  }

 private:
  FileWatcher::Delegate* delegate_;
  FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(FileWatcherImplNotifyTask);
};

class InotifyReaderTask : public Task {
 public:
  InotifyReaderTask(InotifyReader* reader, int inotify_fd, int shutdown_fd)
      : reader_(reader),
        inotify_fd_(inotify_fd),
        shutdown_fd_(shutdown_fd) {
  }

  virtual void Run() {
    while (true) {
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(inotify_fd_, &rfds);
      FD_SET(shutdown_fd_, &rfds);

      // Wait until some inotify events are available.
      int select_result =
        HANDLE_EINTR(select(std::max(inotify_fd_, shutdown_fd_) + 1,
                            &rfds, NULL, NULL, NULL));
      if (select_result < 0) {
        DPLOG(WARNING) << "select failed";
        return;
      }

      if (FD_ISSET(shutdown_fd_, &rfds))
        return;

      // Adjust buffer size to current event queue size.
      int buffer_size;
      int ioctl_result = HANDLE_EINTR(ioctl(inotify_fd_, FIONREAD,
                                            &buffer_size));

      if (ioctl_result != 0) {
        DPLOG(WARNING) << "ioctl failed";
        return;
      }

      std::vector<char> buffer(buffer_size);

      ssize_t bytes_read = HANDLE_EINTR(read(inotify_fd_, &buffer[0],
                                             buffer_size));

      if (bytes_read < 0) {
        DPLOG(WARNING) << "read from inotify fd failed";
        return;
      }

      ssize_t i = 0;
      while (i < bytes_read) {
        inotify_event* event = reinterpret_cast<inotify_event*>(&buffer[i]);
        size_t event_size = sizeof(inotify_event) + event->len;
        DCHECK(i + event_size <= static_cast<size_t>(bytes_read));
        reader_->OnInotifyEvent(event);
        i += event_size;
      }
    }
  }

 private:
  InotifyReader* reader_;
  int inotify_fd_;
  int shutdown_fd_;

  DISALLOW_COPY_AND_ASSIGN(InotifyReaderTask);
};

InotifyReader::InotifyReader()
    : thread_("inotify_reader"),
      inotify_fd_(inotify_init()),
      valid_(false) {
  shutdown_pipe_[0] = -1;
  shutdown_pipe_[1] = -1;
  if (inotify_fd_ >= 0 && pipe(shutdown_pipe_) == 0 && thread_.Start()) {
    thread_.message_loop()->PostTask(
        FROM_HERE, new InotifyReaderTask(this, inotify_fd_, shutdown_pipe_[0]));
    valid_ = true;
  }
}

InotifyReader::~InotifyReader() {
  if (valid_) {
    // Write to the self-pipe so that the select call in InotifyReaderTask
    // returns.
    ssize_t ret = HANDLE_EINTR(write(shutdown_pipe_[1], "", 1));
    DPCHECK(ret > 0);
    DCHECK_EQ(ret, 1);
    thread_.Stop();
  }
  if (inotify_fd_ >= 0)
    close(inotify_fd_);
  if (shutdown_pipe_[0] >= 0)
    close(shutdown_pipe_[0]);
  if (shutdown_pipe_[1] >= 0)
    close(shutdown_pipe_[1]);
}

InotifyReader::Watch InotifyReader::AddWatch(
    const FilePath& path, FileWatcherImpl* watcher) {
  if (!valid_)
    return kInvalidWatch;

  AutoLock auto_lock(lock_);

  Watch watch = inotify_add_watch(inotify_fd_, path.value().c_str(),
                                  IN_CREATE | IN_DELETE |
                                  IN_CLOSE_WRITE | IN_MOVE);

  if (watch == kInvalidWatch)
    return kInvalidWatch;

  watchers_[watch].insert(watcher);

  return watch;
}

bool InotifyReader::RemoveWatch(Watch watch,
                                FileWatcherImpl* watcher) {
  if (!valid_)
    return false;

  AutoLock auto_lock(lock_);

  watchers_[watch].erase(watcher);

  if (watchers_[watch].empty()) {
    watchers_.erase(watch);
    return (inotify_rm_watch(inotify_fd_, watch) == 0);
  }

  return true;
}

void InotifyReader::OnInotifyEvent(const inotify_event* event) {
  if (event->mask & IN_IGNORED)
    return;

  // In case you want to limit the scope of this lock, it's not sufficient
  // to just copy things under the lock, and then run the notifications
  // without holding the lock. FileWatcherImpl's dtor removes its watches,
  // and to do that obtains the lock. After it finishes removing watches,
  // it's destroyed. So, if you copy under the lock and notify without the lock,
  // it's possible you'll copy the FileWatcherImpl which is being
  // destroyed, then it will destroy itself, and then you'll try to notify it.
  AutoLock auto_lock(lock_);

  for (WatcherSet::iterator watcher = watchers_[event->wd].begin();
       watcher != watchers_[event->wd].end();
       ++watcher) {
    (*watcher)->OnInotifyEvent(event);
  }
}

FileWatcherImpl::FileWatcherImpl()
    : watch_(InotifyReader::kInvalidWatch) {
}

FileWatcherImpl::~FileWatcherImpl() {
  if (watch_ == InotifyReader::kInvalidWatch)
    return;

  Singleton<InotifyReader>::get()->RemoveWatch(watch_, this);
}

void FileWatcherImpl::OnInotifyEvent(const inotify_event* event) {
  // Since we're watching the directory, filter out inotify events
  // if it's not related to the file we're watching.
  if (path_ != path_.DirName().Append(event->name))
    return;

  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      new FileWatcherImplNotifyTask(delegate_, path_));
}

bool FileWatcherImpl::Watch(const FilePath& path,
                            FileWatcher::Delegate* delegate) {
  // Each FileWatcherImpl can only watch one file.
  DCHECK(watch_ == InotifyReader::kInvalidWatch);

  // It's not possible to watch a file that doesn't exist, so instead,
  // watch the parent directory.
  if (!file_util::PathExists(path.DirName()))
    return false;

  delegate_ = delegate;
  path_ = path;
  watch_ = Singleton<InotifyReader>::get()->AddWatch(path.DirName(), this);
  return watch_ != InotifyReader::kInvalidWatch;
}

}  // namespace

FileWatcher::FileWatcher() {
  impl_ = new FileWatcherImpl();
}
