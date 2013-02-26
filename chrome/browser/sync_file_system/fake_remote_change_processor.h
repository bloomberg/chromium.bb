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
#include "webkit/fileapi/syncable/sync_callbacks.h"

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

  FakeRemoteChangeProcessor();
  virtual ~FakeRemoteChangeProcessor();

  // RemoteChangeProcessor overrides.
  virtual void PrepareForProcessRemoteChange(
      const fileapi::FileSystemURL& url,
      const std::string& service_name,
      const PrepareChangeCallback& callback) OVERRIDE;
  virtual void ApplyRemoteChange(
      const FileChange& change,
      const base::FilePath& local_path,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback) OVERRIDE;
  virtual void ClearLocalChanges(
      const fileapi::FileSystemURL& url,
      const base::Closure& completion_callback) OVERRIDE;
  virtual void RecordFakeLocalChange(
      const fileapi::FileSystemURL& url,
      const FileChange& change,
      const SyncStatusCallback& callback) OVERRIDE;

  const URLToFileChangesMap& GetAppliedRemoteChanges() const;

 private:
  // History of file changes given by ApplyRemoteChange(). Changes are arranged
  // in chronological order, that is, the end of the vector represents the last
  // change.
  URLToFileChangesMap applied_changes_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoteChangeProcessor);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_
