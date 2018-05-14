// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_UTIL_PROXY_H_
#define BASE_FILES_FILE_UTIL_PROXY_H_

#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"

namespace base {

class TaskRunner;

// This class provides asynchronous access to common file routines.
class BASE_EXPORT FileUtilProxy {
 public:
  using GetFileInfoCallback =
      OnceCallback<void(File::Error, const File::Info&)>;

  // Retrieves the information about a file. It is invalid to pass a null
  // callback.
  // This returns false if task posting to |task_runner| has failed.
  static bool GetFileInfo(TaskRunner* task_runner,
                          const FilePath& file_path,
                          GetFileInfoCallback callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtilProxy);
};

}  // namespace base

#endif  // BASE_FILES_FILE_UTIL_PROXY_H_
