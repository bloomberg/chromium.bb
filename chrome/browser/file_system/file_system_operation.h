// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_H_
#define CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_H_

#include <vector>

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "base/scoped_callback_factory.h"

class FileSystemOperationClient;

// This class is designed to serve one-time file system operation per instance.
// Each operation method (CreateFile, CreateDirectory, Copy, Move,
// DIrectoryExists, GetMetadata, ReadDirectory and Remove) must be called
// only once in its lifetime.
class FileSystemOperation {
 public:
  FileSystemOperation(int request_id, FileSystemOperationClient* client);

  void CreateFile(const FilePath& path,
                  bool exclusive);

  void CreateDirectory(const FilePath& path,
                       bool exclusive);

  void Copy(const FilePath& src_path,
            const FilePath& dest_path);

  // If |dest_path| exists and is a directory, behavior is unspecified or
  // varies for different platforms.
  // TODO(kkanetkar): Add more checks.
  void Move(const FilePath& src_path,
            const FilePath& dest_path);

  void DirectoryExists(const FilePath& path);

  void FileExists(const FilePath& path);

  void GetMetadata(const FilePath& path);

  void ReadDirectory(const FilePath& path);

  void Remove(const FilePath& path);

 private:
  // Callbacks for above methods.
  void DidCreateFileExclusive(
      base::PlatformFileError rv, base::PassPlatformFile file, bool created);

  // Returns success even if the file already existed.
  void DidCreateFileNonExclusive(
      base::PlatformFileError rv, base::PassPlatformFile file, bool created);

  // Generic callback that translates platform errors to WebKit error codes.
  void DidFinishFileOperation(base::PlatformFileError rv);

  void DidDirectoryExists(base::PlatformFileError rv,
                          const base::PlatformFileInfo& file_info);

  void DidFileExists(base::PlatformFileError rv,
                     const base::PlatformFileInfo& file_info);

  void DidGetMetadata(base::PlatformFileError rv,
                      const base::PlatformFileInfo& file_info);

  void DidReadDirectory(
      base::PlatformFileError rv,
      const std::vector<base::file_util_proxy::Entry>& entries);

  // Not owned.
  FileSystemOperationClient* client_;

  // Client's request id.
  int request_id_;

  base::ScopedCallbackFactory<FileSystemOperation> callback_factory_;

  // A flag to make sure we call operation only once per instance.
  bool operation_pending_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperation);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_H_
