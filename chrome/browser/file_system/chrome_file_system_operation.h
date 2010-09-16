// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_CHROME_FILE_SYSTEM_OPERATION_H_
#define CHROME_BROWSER_FILE_SYSTEM_CHROME_FILE_SYSTEM_OPERATION_H_

#include "webkit/fileapi/file_system_operation.h"

// This class is designed to serve one-time file system operation per instance.
// Each operation method (CreateFile, CreateDirectory, Copy, Move,
// DirectoryExists, GetMetadata, ReadDirectory and Remove) must be called
// only once in its lifetime.
class ChromeFileSystemOperation : public fileapi::FileSystemOperation {
 public:
  ChromeFileSystemOperation(
      int request_id, fileapi::FileSystemOperationClient* client);

  int request_id() const { return request_id_; }

 private:
  int request_id_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFileSystemOperation);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_CHROME_FILE_SYSTEM_OPERATION_H_

