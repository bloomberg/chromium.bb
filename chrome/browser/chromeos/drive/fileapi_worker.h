// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_WORKER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_WORKER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/platform_file.h"
#include "webkit/common/blob/scoped_file.h"

namespace base {
class FilePath;
}  // namespace base

namespace fileapi {
struct DirectoryEntry;
}  // namespace fileapi

namespace drive {

class FileSystemInterface;

namespace internal {

// This provides the core implementation of the methods for fileapi.
// This class lives on UI thread. Note that most method invocation of fileapi
// is done on IO thread. The gap is filled by FileSystemProxy.
class FileApiWorker {
 public:
  typedef base::Callback<
      void(base::PlatformFileError result)> StatusCallback;
  typedef base::Callback<
      void(base::PlatformFileError result,
           const base::PlatformFileInfo& file_info)> GetFileInfoCallback;
  typedef base::Callback<
      void(base::PlatformFileError result,
           const std::vector<fileapi::DirectoryEntry>& file_list,
           bool has_more)> ReadDirectoryCallback;
  typedef base::Callback<
      void(base::PlatformFileError result,
           const base::PlatformFileInfo& file_info,
           const base::FilePath& snapshot_file_path,
           webkit_blob::ScopedFile::ScopeOutPolicy scope_out_policy)>
      CreateSnapshotFileCallback;

  // |file_system| must not be NULL.
  explicit FileApiWorker(FileSystemInterface* file_system);
  ~FileApiWorker();

  FileSystemInterface* file_system() { return file_system_; }

  // Returns the metadata info of the file at |file_path|.
  // Called from FileSystemProxy::GetFileInfo().
  void GetFileInfo(const base::FilePath& file_path,
                   const GetFileInfoCallback& callback);

  // Copies a file from |src_file_path| to |dest_file_path|.
  // Called from FileSystemProxy::Copy().
  void Copy(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            const StatusCallback& callback);

  // Moves a file from |src_file_path| to |dest_file_path|.
  // Called from FileSystemProxy::Move().
  void Move(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            const StatusCallback& callback);

  // Reads the contents of the directory at |file_path|.
  // Called from FileSystemProxy::ReadDirectory().
  void ReadDirectory(const base::FilePath& file_path,
                     const ReadDirectoryCallback& callback);

  // Removes a file at |file_path|. Called from FileSystemProxy::Remove().
  void Remove(const base::FilePath& file_path,
              bool is_recursive,
              const StatusCallback& callback);

  // Creates a new directory at |file_path|.
  // Called from FileSystemProxy::CreateDirectory().
  void CreateDirectory(const base::FilePath& file_path,
                       bool is_exclusive,
                       bool is_recursive,
                       const StatusCallback& callback);

  // Creates a new file at |file_path|.
  // Called from FileSystemProxy::CreateFile().
  void CreateFile(const base::FilePath& file_path,
                  bool is_exclusive,
                  const StatusCallback& callback);

  // Truncates the file at |file_path| to |length| bytes.
  // Called from FileSystemProxy::Truncate().
  void Truncate(const base::FilePath& file_path,
                int64 length,
                const StatusCallback& callback);

  // Creates a snapshot for the file at |file_path|.
  // Called from FileSystemProxy::CreateSnapshotFile().
  void CreateSnapshotFile(const base::FilePath& file_path,
                          const CreateSnapshotFileCallback& callback);

  // Changes timestamp of the file at |file_path| to |last_access_time| and
  // |last_modified_time|. Called from FileSystemProxy::TouchFile().
  void TouchFile(const base::FilePath& file_path,
                 const base::Time& last_access_time,
                 const base::Time& last_modified_time,
                 const StatusCallback& callback);

 private:
  FileSystemInterface* file_system_;

  DISALLOW_COPY_AND_ASSIGN(FileApiWorker);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_WORKER_H_
