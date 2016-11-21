// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_FILE_PATH_WATCHER_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_FILE_PATH_WATCHER_UTIL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "content/public/browser/browser_thread.h"

using MediaFilePathWatcherUniquePtr =
    std::unique_ptr<base::FilePathWatcher,
                    content::BrowserThread::DeleteOnFileThread>;
using FileWatchStartedCallback =
    base::Callback<void(MediaFilePathWatcherUniquePtr watcher)>;

// Called on the MediaTaskRunner to begin a file watch.
// |watch_started_callback| is responsible for taking ownership of the
// file watcher upon start. All callbacks are run on the MediaTaskRunner.
void StartFilePathWatchOnMediaTaskRunner(
    const base::FilePath& path,
    const FileWatchStartedCallback& watch_started_callback,
    const base::FilePathWatcher::Callback& path_changed_callback);

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_FILE_PATH_WATCHER_UTIL_H_
