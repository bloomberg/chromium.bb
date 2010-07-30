// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_PROXY_H_
#define CHROME_BROWSER_FILE_SYSTEM_PROXY_H_

#include "base/callback.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"

// This class provides asynchronous access to common file routines.
class FileSystemProxy {
 public:
  // This callback is used by methods that report success with a bool.  It is
  // valid to pass NULL as the callback parameter to any function that takes a
  // StatusCallback, in which case the operation will complete silently.
  typedef Callback1<bool /* succeeded */>::Type StatusCallback;

  // Creates or opens a file with the given flags.  It is invalid to pass NULL
  // for the callback.
  typedef Callback2<base::PassPlatformFile, bool /* created */>::Type
      CreateOrOpenCallback;
  static void CreateOrOpen(const FilePath& file_path,
                           int file_flags,
                           CreateOrOpenCallback* callback);

  // Creates a temporary file for writing.  The path and an open file handle
  // are returned.  It is invalid to pass NULL for the callback.
  typedef Callback2<base::PassPlatformFile, FilePath>::Type
      CreateTemporaryCallback;
  static void CreateTemporary(CreateTemporaryCallback* callback);

  // Close the given file handle.
  static void Close(base::PlatformFile, StatusCallback* callback);

  // Deletes a file or empty directory.
  static void Delete(const FilePath& file_path,
                     StatusCallback* callback);

  // Deletes a directory and all of its contents.
  static void RecursiveDelete(const FilePath& file_path,
                              StatusCallback* callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemProxy);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_PROXY_H_
