// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/debug_info_collector.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/google_apis/task_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

void IterateFileCacheInternal(
    internal::FileCache* file_cache,
    const DebugInfoCollector::IterateFileCacheCallback& iteration_callback) {
  scoped_ptr<internal::FileCache::Iterator> it = file_cache->GetIterator();
  for (; !it->IsAtEnd(); it->Advance())
    iteration_callback.Run(it->GetID(), it->GetValue());
  DCHECK(!it->HasError());
}

}  // namespace

DebugInfoCollector::DebugInfoCollector(
    FileSystemInterface* file_system,
    internal::FileCache* file_cache,
    base::SequencedTaskRunner* blocking_task_runner)
    : file_system_(file_system),
      file_cache_(file_cache),
      blocking_task_runner_(blocking_task_runner) {
  DCHECK(file_system_);
  DCHECK(file_cache_);
}

DebugInfoCollector::~DebugInfoCollector() {
}

void DebugInfoCollector::IterateFileCache(
    const IterateFileCacheCallback& iteration_callback,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!iteration_callback.is_null());
  DCHECK(!completion_callback.is_null());

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&IterateFileCacheInternal,
                 file_cache_,
                 google_apis::CreateRelayCallback(iteration_callback)),
      completion_callback);
}

void DebugInfoCollector::GetMetadata(
    const GetFilesystemMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Currently, this is just a proxy to the FileSystem.
  // TODO(hidehiko): Move the implementation to here to simplify the
  // FileSystem's implementation. crbug.com/237088
  file_system_->GetMetadata(callback);
}

}  // namespace drive
