// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {

FakeRemoteChangeProcessor::FakeRemoteChangeProcessor() {
}

FakeRemoteChangeProcessor::~FakeRemoteChangeProcessor() {
}

void FakeRemoteChangeProcessor::PrepareForProcessRemoteChange(
    const fileapi::FileSystemURL& url,
    const PrepareChangeCallback& callback) {
  SyncFileMetadata local_metadata;

  URLToFileMetadata::iterator found_metadata = local_file_metadata_.find(url);
  if (found_metadata != local_file_metadata_.end())
    local_metadata = found_metadata->second;

  // Override |local_metadata| by applied changes.
  URLToFileChangesMap::iterator found = applied_changes_.find(url);
  if (found != applied_changes_.end()) {
    DCHECK(!found->second.empty());
    const FileChange& applied_change = found->second.back();
    if (applied_change.IsAddOrUpdate()) {
      local_metadata = SyncFileMetadata(
          applied_change.file_type(),
          100 /* size */,
          base::Time::Now());
    }
  }

  FileChangeList change_list;
  URLToFileChangeList::iterator found_list = local_changes_.find(url);
  if (found_list != local_changes_.end())
    change_list = found_list->second;

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_STATUS_OK,
                 local_metadata, change_list));
}

void FakeRemoteChangeProcessor::ApplyRemoteChange(
    const FileChange& change,
    const base::FilePath& local_path,
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  SyncStatusCode status = SYNC_STATUS_UNKNOWN;
  base::FilePath ancestor = fileapi::VirtualPath::DirName(url.path());
  while (true) {
    fileapi::FileSystemURL ancestor_url =
        CreateSyncableFileSystemURL(url.origin(), ancestor);
    if (!ancestor_url.is_valid())
      break;

    URLToFileChangeList::iterator found_list =
        local_changes_.find(ancestor_url);
    if (found_list != local_changes_.end()) {
      const FileChange& local_change = found_list->second.back();
      if (local_change.IsAddOrUpdate() &&
          local_change.file_type() != SYNC_FILE_TYPE_DIRECTORY) {
        status = SYNC_FILE_ERROR_NOT_A_DIRECTORY;
        break;
      }
    }

    base::FilePath ancestor_parent = fileapi::VirtualPath::DirName(ancestor);
    if (ancestor == ancestor_parent)
      break;
    ancestor = ancestor_parent;
  }
  if (status == SYNC_STATUS_UNKNOWN) {
    applied_changes_[url].push_back(change);
    status = SYNC_STATUS_OK;
  }
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, status));
}

void FakeRemoteChangeProcessor::FinalizeRemoteSync(
    const fileapi::FileSystemURL& url,
    bool clear_local_changes,
    const base::Closure& completion_callback) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE, completion_callback);
}

void FakeRemoteChangeProcessor::RecordFakeLocalChange(
    const fileapi::FileSystemURL& url,
    const FileChange& change,
    const SyncStatusCallback& callback) {
  local_changes_[url].Update(change);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
}

void FakeRemoteChangeProcessor::UpdateLocalFileMetadata(
    const fileapi::FileSystemURL& url,
    const FileChange& change) {
  if (change.IsAddOrUpdate()) {
    local_file_metadata_[url] = SyncFileMetadata(
        change.file_type(), 100 /* size */, base::Time::Now());
  } else {
    local_file_metadata_.erase(url);
  }
  local_changes_[url].Update(change);
}

const FakeRemoteChangeProcessor::URLToFileChangesMap&
FakeRemoteChangeProcessor::GetAppliedRemoteChanges() const {
  return applied_changes_;
}

}  // namespace sync_file_system
