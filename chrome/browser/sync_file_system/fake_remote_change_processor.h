// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"

namespace base {
class FilePath;
}

namespace fileapi {
class FileSystemURL;
}

namespace sync_file_system {

class FileChange;

class FakeRemoteChangeProcessor : public RemoteChangeProcessor {
 public:
  typedef std::map<fileapi::FileSystemURL, std::vector<FileChange>,
                   fileapi::FileSystemURL::Comparator> URLToFileChangesMap;
  typedef std::map<fileapi::FileSystemURL, FileChangeList,
                   fileapi::FileSystemURL::Comparator> URLToFileChangeList;
  typedef std::map<fileapi::FileSystemURL, SyncFileMetadata,
                   fileapi::FileSystemURL::Comparator> URLToFileMetadata;

  FakeRemoteChangeProcessor();
  virtual ~FakeRemoteChangeProcessor();

  // RemoteChangeProcessor overrides.
  virtual void PrepareForProcessRemoteChange(
      const fileapi::FileSystemURL& url,
      const PrepareChangeCallback& callback) OVERRIDE;
  virtual void ApplyRemoteChange(
      const FileChange& change,
      const base::FilePath& local_path,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback) OVERRIDE;
  virtual void FinalizeRemoteSync(
      const fileapi::FileSystemURL& url,
      bool clear_local_changes,
      const base::Closure& completion_callback) OVERRIDE;
  virtual void RecordFakeLocalChange(
      const fileapi::FileSystemURL& url,
      const FileChange& change,
      const SyncStatusCallback& callback) OVERRIDE;

  void UpdateLocalFileMetadata(
    const fileapi::FileSystemURL& url,
    const FileChange& change);
  void ClearLocalChanges(const fileapi::FileSystemURL& url);

  const URLToFileChangesMap& GetAppliedRemoteChanges() const;

  // Compare |applied_changes_| with |expected_changes|.
  // This internally calls EXPECT_FOO, ASSERT_FOO methods in the
  // verification.
  void VerifyConsistency(const URLToFileChangesMap& expected_changes);

 private:
  // History of file changes given by ApplyRemoteChange(). Changes are arranged
  // in chronological order, that is, the end of the vector represents the last
  // change.
  URLToFileChangesMap applied_changes_;

  // History of local file changes.
  URLToFileChangeList local_changes_;

  // Initial local file metadata. These are overridden by applied changes.
  URLToFileMetadata local_file_metadata_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoteChangeProcessor);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_
