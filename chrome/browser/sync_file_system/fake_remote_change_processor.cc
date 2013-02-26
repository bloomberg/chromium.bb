// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"

namespace sync_file_system {

FakeRemoteChangeProcessor::FakeRemoteChangeProcessor() {
}

FakeRemoteChangeProcessor::~FakeRemoteChangeProcessor() {
}

void FakeRemoteChangeProcessor::PrepareForProcessRemoteChange(
    const fileapi::FileSystemURL& url,
    const std::string& service_name,
    const PrepareChangeCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_STATUS_OK,
                 SyncFileMetadata(), FileChangeList()));
}

void FakeRemoteChangeProcessor::ApplyRemoteChange(
    const FileChange& change,
    const base::FilePath& local_path,
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  applied_changes_[url].push_back(change);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
}

void FakeRemoteChangeProcessor::ClearLocalChanges(
    const fileapi::FileSystemURL& url,
    const base::Closure& completion_callback) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE, completion_callback);
}

void FakeRemoteChangeProcessor::RecordFakeLocalChange(
    const fileapi::FileSystemURL& url,
    const FileChange& change,
    const SyncStatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
}

const FakeRemoteChangeProcessor::URLToFileChangesMap&
FakeRemoteChangeProcessor::GetAppliedRemoteChanges() const {
  return applied_changes_;
}

}  // namespace sync_file_system
