// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/file_path_watcher_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Bounces |path| and |error| to |callback| from the FILE thread to the media
// task runner.
void OnFilePathChangedOnFileThread(
    const base::FilePathWatcher::Callback& callback,
    const base::FilePath& path,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
      FROM_HERE, base::Bind(callback, path, error));
}

// The watch has to be started on the FILE thread, and the callback called by
// the FilePathWatcher also needs to run on the FILE thread.
void StartFilePathWatchOnFileThread(
    const base::FilePath& path,
    const FileWatchStartedCallback& watch_started_callback,
    const base::FilePathWatcher::Callback& path_changed_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  // The watcher is created on the FILE thread because it is very difficult
  // to safely pass an already-created file watcher to a different thread.
  scoped_ptr<base::FilePathWatcher> watcher(new base::FilePathWatcher);
  bool success = watcher->Watch(
      path,
      false /* recursive */,
      base::Bind(&OnFilePathChangedOnFileThread, path_changed_callback));
  if (!success)
    LOG(ERROR) << "Adding watch for " << path.value() << " failed";
  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
      FROM_HERE, base::Bind(watch_started_callback, base::Passed(&watcher)));
}

}  // namespace

void StartFilePathWatchOnMediaTaskRunner(
    const base::FilePath& path,
    const FileWatchStartedCallback& watch_started_callback,
    const base::FilePathWatcher::Callback& path_changed_callback) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  content::BrowserThread::PostTask(content::BrowserThread::FILE,
                                   FROM_HERE,
                                   base::Bind(&StartFilePathWatchOnFileThread,
                                              path,
                                              watch_started_callback,
                                              path_changed_callback));
}
