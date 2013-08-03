// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_watcher_extensions.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/google_apis/task_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace file_manager {
namespace {

// Creates a base::FilePathWatcher and starts watching at |watch_path| with
// |callback|. Returns NULL on failure.
base::FilePathWatcher* CreateAndStartFilePathWatcher(
    const base::FilePath& watch_path,
    const base::FilePathWatcher::Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!callback.is_null());

  scoped_ptr<base::FilePathWatcher> watcher(new base::FilePathWatcher);
  if (!watcher->Watch(watch_path, false /* recursive */, callback))
    return NULL;

  return watcher.release();
}

}  // namespace

FileWatcherExtensions::FileWatcherExtensions(const base::FilePath& virtual_path,
                                             const std::string& extension_id,
                                             bool is_remote_file_system)
    : file_watcher_(NULL),
      virtual_path_(virtual_path),
      ref_count_(0),
      is_remote_file_system_(is_remote_file_system),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AddExtension(extension_id);
}

FileWatcherExtensions::~FileWatcherExtensions() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, file_watcher_);
}

void FileWatcherExtensions::AddExtension(const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  extensions_[extension_id]++;
  ref_count_++;
}

void FileWatcherExtensions::RemoveExtension(const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionUsageRegistry::iterator it = extensions_.find(extension_id);
  if (it == extensions_.end()) {
    LOG(ERROR) << " Extension [" << extension_id
               << "] tries to unsubscribe from folder [" << local_path_.value()
               << "] it isn't subscribed";
    return;
  }

  // If entry found - decrease it's count and remove if necessary
  if (it->second-- == 0)
    extensions_.erase(it);

  ref_count_--;
}

void FileWatcherExtensions::Watch(
    const base::FilePath& local_path,
    const base::FilePathWatcher::Callback& file_watcher_callback,
    const BoolCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!file_watcher_);

  local_path_ = local_path;  // For error message in RemoveExtension().

  if (is_remote_file_system_) {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
    return;
  }

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateAndStartFilePathWatcher,
                 local_path,
                 google_apis::CreateRelayCallback(file_watcher_callback)),
      base::Bind(&FileWatcherExtensions::OnWatcherStarted,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileWatcherExtensions::OnWatcherStarted(
    const BoolCallback& callback,
    base::FilePathWatcher* file_watcher) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!file_watcher_);

  if (file_watcher) {
    file_watcher_ = file_watcher;
    callback.Run(true);
  } else {
    callback.Run(false);
  }
}

}  // namespace file_manager
