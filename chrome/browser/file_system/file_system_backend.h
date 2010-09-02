// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_BACKEND_H_
#define CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_BACKEND_H_

#include <vector>

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/scoped_callback_factory.h"
#include "chrome/browser/chrome_thread.h"

class FileSystemBackendClient;

class FileSystemBackend {
 public:
  FileSystemBackend();

  void set_client(FileSystemBackendClient* client);

  void CreateFile(const FilePath& path,
                  bool exclusive,
                  int request_id);

  void CreateDirectory(const FilePath& path,
                       bool exclusive,
                       int request_id);

  void Copy(const FilePath& src_path,
            const FilePath& dest_path,
            int request_id);

  void Move(const FilePath& src_path,
            const FilePath& dest_path,
            int request_id);

  void DirectoryExists(const FilePath& path, int request_id);

  void FileExists(const FilePath& path, int request_id);

  void GetMetadata(const FilePath& path, int request_id);

  void ReadDirectory(const FilePath& path, int request_id);

  void Remove(const FilePath& path, int request_id);

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
                          const file_util::FileInfo& file_info);

  void DidFileExists(base::PlatformFileError rv,
                     const file_util::FileInfo& file_info);

  void DidGetMetadata(base::PlatformFileError rv,
                      const file_util::FileInfo& file_info);

  void DidReadDirectory(
      base::PlatformFileError rv,
      const std::vector<base::file_util_proxy::Entry>& entries);

  // Not owned.
  FileSystemBackendClient* client_;
  // Client's request id.
  int request_id_;
  base::ScopedCallbackFactory<FileSystemBackend> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemBackend);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_BACKEND_H_
