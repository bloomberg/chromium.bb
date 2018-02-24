// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util_proxy.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"

namespace base {

namespace {

void CallWithTranslatedParameter(FileUtilProxy::StatusCallback callback,
                                 bool value) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(value ? File::FILE_OK : File::FILE_ERROR_FAILED);
}

class GetFileInfoHelper {
 public:
  GetFileInfoHelper()
      : error_(File::FILE_OK) {}

  void RunWorkForFilePath(const FilePath& file_path) {
    if (!PathExists(file_path)) {
      error_ = File::FILE_ERROR_NOT_FOUND;
      return;
    }
    if (!GetFileInfo(file_path, &file_info_))
      error_ = File::FILE_ERROR_FAILED;
  }

  void Reply(FileUtilProxy::GetFileInfoCallback callback) {
    if (!callback.is_null()) {
      std::move(callback).Run(error_, file_info_);
    }
  }

 private:
  File::Error error_;
  File::Info file_info_;
  DISALLOW_COPY_AND_ASSIGN(GetFileInfoHelper);
};

}  // namespace

// Retrieves the information about a file. It is invalid to pass NULL for the
// callback.
bool FileUtilProxy::GetFileInfo(TaskRunner* task_runner,
                                const FilePath& file_path,
                                GetFileInfoCallback callback) {
  GetFileInfoHelper* helper = new GetFileInfoHelper;
  return task_runner->PostTaskAndReply(
      FROM_HERE,
      BindOnce(&GetFileInfoHelper::RunWorkForFilePath, Unretained(helper),
               file_path),
      BindOnce(&GetFileInfoHelper::Reply, Owned(helper), std::move(callback)));
}

// static
bool FileUtilProxy::Touch(TaskRunner* task_runner,
                          const FilePath& file_path,
                          const Time& last_access_time,
                          const Time& last_modified_time,
                          StatusCallback callback) {
  return base::PostTaskAndReplyWithResult(
      task_runner, FROM_HERE,
      BindOnce(&TouchFile, file_path, last_access_time, last_modified_time),
      BindOnce(&CallWithTranslatedParameter, std::move(callback)));
}

}  // namespace base
