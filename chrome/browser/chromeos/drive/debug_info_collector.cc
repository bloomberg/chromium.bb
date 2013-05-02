// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/debug_info_collector.h"

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

DebugInfoCollector::DebugInfoCollector(FileSystemInterface* file_system,
                                       FileCache* file_cache)
    : file_system_(file_system),
      file_cache_(file_cache) {
  DCHECK(file_system_);
  DCHECK(file_cache_);
}

DebugInfoCollector::~DebugInfoCollector() {
}

void DebugInfoCollector::IterateFileCache(
    const CacheIterateCallback& iteration_callback,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!iteration_callback.is_null());
  DCHECK(!completion_callback.is_null());

  file_cache_->Iterate(iteration_callback, completion_callback);
}

void DebugInfoCollector::GetMetadata(
    const GetFilesystemMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Currently, this is just a proxy to the DriveFileSystem.
  // TODO(hidehiko): Move the implementation to here to simplify the
  // DriveFileSystem's implementation. crbug.com/237088
  file_system_->GetMetadata(callback);
}

}  // namespace drive
