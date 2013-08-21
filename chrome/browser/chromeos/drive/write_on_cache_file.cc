// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/write_on_cache_file.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

// Runs |file_io_task_callback| in blocking pool and runs |close_callback|
// in the UI thread after that.
void WriteOnCacheFileAfterOpenFile(
    const base::FilePath& drive_path,
    const WriteOnCacheFileCallback& file_io_task_callback,
    FileError error,
    const base::FilePath& local_cache_path,
    const base::Closure& close_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == FILE_ERROR_OK) {
    DCHECK(!close_callback.is_null());
    BrowserThread::GetBlockingPool()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(file_io_task_callback, error, local_cache_path),
        close_callback);
  } else {
    BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE,
        base::Bind(file_io_task_callback, error, local_cache_path));

  }
}

}  // namespace

void WriteOnCacheFile(FileSystemInterface* file_system,
                      const base::FilePath& path,
                      const std::string& mime_type,
                      const WriteOnCacheFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(file_system);
  DCHECK(!callback.is_null());

  file_system->OpenFile(
      path,
      OPEN_OR_CREATE_FILE,
      mime_type,
      base::Bind(&WriteOnCacheFileAfterOpenFile, path, callback));
}

}  // namespace drive
