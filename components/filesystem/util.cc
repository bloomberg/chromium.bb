// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <limits>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/string.h"

namespace filesystem {

Error IsPathValid(const mojo::String& path) {
  DCHECK(!path.is_null());
  if (!base::IsStringUTF8(path.get()))
    return ERROR_INVALID_ARGUMENT;
  if (path.size() > 0 && path[0] == '/')
    return ERROR_PERMISSION_DENIED;
  return ERROR_OK;
}

Error IsWhenceValid(Whence whence) {
  return (whence == WHENCE_FROM_CURRENT || whence == WHENCE_FROM_START ||
          whence == WHENCE_FROM_END)
             ? ERROR_OK
             : ERROR_UNIMPLEMENTED;
}

Error IsOffsetValid(int64_t offset) {
  return (offset >= std::numeric_limits<off_t>::min() &&
          offset <= std::numeric_limits<off_t>::max())
             ? ERROR_OK
             : ERROR_OUT_OF_RANGE;
}

Error ErrnoToError(int errno_value) {
  // TODO(vtl)
  return ERROR_UNKNOWN;
}

int WhenceToStandardWhence(Whence whence) {
  DCHECK_EQ(IsWhenceValid(whence), ERROR_OK);
  switch (whence) {
    case WHENCE_FROM_CURRENT:
      return SEEK_CUR;
    case WHENCE_FROM_START:
      return SEEK_SET;
    case WHENCE_FROM_END:
      return SEEK_END;
  }
  NOTREACHED();
  return 0;
}

Error TimespecToStandardTimespec(const Timespec* in, struct timespec* out) {
  if (!in) {
    out->tv_sec = 0;
    out->tv_nsec = UTIME_OMIT;
    return ERROR_OK;
  }

  static_assert(sizeof(int64_t) >= sizeof(time_t), "whoa, time_t is huge");
  if (in->seconds < std::numeric_limits<time_t>::min() ||
      in->seconds > std::numeric_limits<time_t>::max())
    return ERROR_OUT_OF_RANGE;

  if (in->nanoseconds < 0 || in->nanoseconds >= 1000000000)
    return ERROR_INVALID_ARGUMENT;

  out->tv_sec = static_cast<time_t>(in->seconds);
  out->tv_nsec = static_cast<long>(in->nanoseconds);
  return ERROR_OK;
}

Error TimespecOrNowToStandardTimespec(const TimespecOrNow* in,
                                      struct timespec* out) {
  if (!in) {
    out->tv_sec = 0;
    out->tv_nsec = UTIME_OMIT;
    return ERROR_OK;
  }

  if (in->now) {
    if (!in->timespec.is_null())
      return ERROR_INVALID_ARGUMENT;
    out->tv_sec = 0;
    out->tv_nsec = UTIME_NOW;
    return ERROR_OK;
  }

  return TimespecToStandardTimespec(in->timespec.get(), out);
}

Error ValidateOpenFlags(uint32_t open_flags, bool is_directory) {
  // Treat unknown flags as "unimplemented".
  if ((open_flags &
       ~(kOpenFlagRead | kOpenFlagWrite | kOpenFlagCreate | kOpenFlagExclusive |
         kOpenFlagAppend | kOpenFlagTruncate)))
    return ERROR_UNIMPLEMENTED;

  // At least one of |kOpenFlagRead| or |kOpenFlagWrite| must be set.
  if (!(open_flags & (kOpenFlagRead | kOpenFlagWrite)))
    return ERROR_INVALID_ARGUMENT;

  // |kOpenFlagCreate| requires |kOpenFlagWrite|.
  if ((open_flags & kOpenFlagCreate) && !(open_flags & kOpenFlagWrite))
    return ERROR_INVALID_ARGUMENT;

  // |kOpenFlagExclusive| requires |kOpenFlagCreate|.
  if ((open_flags & kOpenFlagExclusive) && !(open_flags & kOpenFlagCreate))
    return ERROR_INVALID_ARGUMENT;

  if (is_directory) {
    // Check that file-only flags aren't set.
    if ((open_flags & (kOpenFlagAppend | kOpenFlagTruncate)))
      return ERROR_INVALID_ARGUMENT;
    return ERROR_OK;
  }

  // File-only flags:

  // |kOpenFlagAppend| requires |kOpenFlagWrite|.
  if ((open_flags & kOpenFlagAppend) && !(open_flags & kOpenFlagWrite))
    return ERROR_INVALID_ARGUMENT;

  // |kOpenFlagTruncate| requires |kOpenFlagWrite|.
  if ((open_flags & kOpenFlagTruncate) && !(open_flags & kOpenFlagWrite))
    return ERROR_INVALID_ARGUMENT;

  return ERROR_OK;
}

Error ValidateDeleteFlags(uint32_t delete_flags) {
  // Treat unknown flags as "unimplemented".
  if ((delete_flags &
       ~(kDeleteFlagFileOnly | kDeleteFlagDirectoryOnly |
         kDeleteFlagRecursive)))
    return ERROR_UNIMPLEMENTED;

  // Only one of the three currently-defined flags may be set.
  if ((delete_flags & kDeleteFlagFileOnly) &&
      (delete_flags & (kDeleteFlagDirectoryOnly | kDeleteFlagRecursive)))
    return ERROR_INVALID_ARGUMENT;
  if ((delete_flags & kDeleteFlagDirectoryOnly) &&
      (delete_flags & (kDeleteFlagFileOnly | kDeleteFlagRecursive)))
    return ERROR_INVALID_ARGUMENT;
  if ((delete_flags & kDeleteFlagRecursive) &&
      (delete_flags & (kDeleteFlagFileOnly | kDeleteFlagDirectoryOnly)))
    return ERROR_INVALID_ARGUMENT;

  return ERROR_OK;
}

}  // namespace filesystem
