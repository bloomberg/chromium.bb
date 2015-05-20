// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/shared_impl.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "base/logging.h"
#include "components/filesystem/futimens.h"
#include "components/filesystem/util.h"

namespace mojo {
namespace files {

void StatFD(int fd, FileType type, const StatFDCallback& callback) {
  DCHECK_NE(fd, -1);

  struct stat buf;
  if (fstat(fd, &buf) != 0) {
    callback.Run(ErrnoToError(errno), nullptr);
    return;
  }

  FileInformationPtr file_info(FileInformation::New());
  file_info->type = type;
  // Only fill in |size| for files.
  if (S_ISREG(buf.st_mode)) {
    file_info->size = static_cast<int64_t>(buf.st_size);
  } else {
    LOG_IF(WARNING, !S_ISDIR(buf.st_mode))
        << "Unexpected fstat() of special file";
    file_info->size = 0;
  }
  file_info->atime = Timespec::New();
  file_info->mtime = Timespec::New();
#if defined(OS_ANDROID)
  file_info->atime->seconds = static_cast<int64_t>(buf.st_atime);
  file_info->atime->nanoseconds = static_cast<int32_t>(buf.st_atime_nsec);
  file_info->mtime->seconds = static_cast<int64_t>(buf.st_mtime);
  file_info->mtime->nanoseconds = static_cast<int32_t>(buf.st_mtime_nsec);
#else
  file_info->atime->seconds = static_cast<int64_t>(buf.st_atim.tv_sec);
  file_info->atime->nanoseconds = static_cast<int32_t>(buf.st_atim.tv_nsec);
  file_info->mtime->seconds = static_cast<int64_t>(buf.st_mtim.tv_sec);
  file_info->mtime->nanoseconds = static_cast<int32_t>(buf.st_mtim.tv_nsec);
#endif

  callback.Run(ERROR_OK, file_info.Pass());
}

void TouchFD(int fd,
             TimespecOrNowPtr atime,
             TimespecOrNowPtr mtime,
             const TouchFDCallback& callback) {
  DCHECK_NE(fd, -1);

  struct timespec times[2];
  if (Error error = TimespecOrNowToStandardTimespec(atime.get(), &times[0])) {
    callback.Run(error);
    return;
  }
  if (Error error = TimespecOrNowToStandardTimespec(mtime.get(), &times[1])) {
    callback.Run(error);
    return;
  }

  if (futimens(fd, times) != 0) {
    callback.Run(ErrnoToError(errno));
    return;
  }

  callback.Run(ERROR_OK);
}

}  // namespace files
}  // namespace mojo
