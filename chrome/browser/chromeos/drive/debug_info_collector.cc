// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/debug_info_collector.h"

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/task_util.h"

using content::BrowserThread;

namespace drive {

namespace {

void IterateFileCacheInternal(
    internal::ResourceMetadata* metadata,
    const DebugInfoCollector::IterateFileCacheCallback& iteration_callback) {
  scoped_ptr<internal::ResourceMetadata::Iterator> it = metadata->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    if (it->GetValue().file_specific_info().has_cache_state()) {
      iteration_callback.Run(it->GetID(),
                             it->GetValue().file_specific_info().cache_state());
    }
  }
  DCHECK(!it->HasError());
}

// Runs the callback with arguments.
void RunGetResourceEntryCallback(const GetResourceEntryCallback& callback,
                                 scoped_ptr<ResourceEntry> entry,
                                 FileError error) {
  DCHECK(!callback.is_null());
  if (error != FILE_ERROR_OK)
    entry.reset();
  callback.Run(error, entry.Pass());
}

// Runs the callback with arguments.
void RunReadDirectoryCallback(
    const DebugInfoCollector::ReadDirectoryCallback& callback,
    scoped_ptr<ResourceEntryVector> entries,
    FileError error) {
  DCHECK(!callback.is_null());
  if (error != FILE_ERROR_OK)
    entries.reset();
  callback.Run(error, entries.Pass());
}

}  // namespace

DebugInfoCollector::DebugInfoCollector(
    internal::ResourceMetadata* metadata,
    FileSystemInterface* file_system,
    base::SequencedTaskRunner* blocking_task_runner)
    : metadata_(metadata),
      file_system_(file_system),
      blocking_task_runner_(blocking_task_runner) {
  DCHECK(metadata_);
  DCHECK(file_system_);
}

DebugInfoCollector::~DebugInfoCollector() {
}

void DebugInfoCollector::GetResourceEntry(
    const base::FilePath& file_path,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  ResourceEntry* entry_ptr = entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(metadata_),
                 file_path,
                 entry_ptr),
      base::Bind(&RunGetResourceEntryCallback, callback, base::Passed(&entry)));
}

void DebugInfoCollector::ReadDirectory(
    const base::FilePath& file_path,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<ResourceEntryVector> entries(new ResourceEntryVector);
  ResourceEntryVector* entries_ptr = entries.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::ReadDirectoryByPath,
                 base::Unretained(metadata_),
                 file_path,
                 entries_ptr),
      base::Bind(&RunReadDirectoryCallback, callback, base::Passed(&entries)));
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
                 metadata_,
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
