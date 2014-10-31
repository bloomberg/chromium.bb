// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/watcher_manager.h"

#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {

WatcherManager::WatcherManager() {
}
WatcherManager::~WatcherManager() {
}

void WatcherManager::AddWatcher(
    const storage::FileSystemURL& url,
    bool recursive,
    const StatusCallback& callback,
    const NotificationCallback& notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  parser.file_system()->AddWatcher(url.origin(),
                                   parser.file_path(),
                                   recursive,
                                   false /* persistent */,
                                   callback,
                                   notification_callback);
}

void WatcherManager::RemoveWatcher(const storage::FileSystemURL& url,
                                   bool recursive,
                                   const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  parser.file_system()->RemoveWatcher(
      url.origin(), parser.file_path(), recursive, callback);
}

}  // namespace file_system_provider
}  // namespace chromeos
