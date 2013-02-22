// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"

namespace base {
class FilePath;
}

namespace fileapi {
class FileChange;
class FileSystemURL;
}

namespace sync_file_system {

class FakeRemoteChangeProcessor : public RemoteChangeProcessor {
 public:
  FakeRemoteChangeProcessor();
  virtual ~FakeRemoteChangeProcessor();

  // RemoteChangeProcessor overrides.
  virtual void PrepareForProcessRemoteChange(
      const fileapi::FileSystemURL& url,
      const std::string& service_name,
      const PrepareChangeCallback& callback) OVERRIDE;
  virtual void ApplyRemoteChange(
      const fileapi::FileChange& change,
      const base::FilePath& local_path,
      const fileapi::FileSystemURL& url,
      const fileapi::SyncStatusCallback& callback) OVERRIDE;
  virtual void ClearLocalChanges(
      const fileapi::FileSystemURL& url,
      const base::Closure& completion_callback) OVERRIDE;
  virtual void RecordFakeLocalChange(
      const fileapi::FileSystemURL& url,
      const fileapi::FileChange& change,
      const fileapi::SyncStatusCallback& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeRemoteChangeProcessor);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_
