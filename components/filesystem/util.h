// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_FILES_UTIL_H_
#define SERVICES_FILES_UTIL_H_

#include "components/filesystem/public/interfaces/types.mojom.h"

namespace mojo {

class String;

namespace files {

// Validation functions (typically used to check arguments; they return
// |ERROR_OK| if valid, else the standard/recommended error for the validation
// error):

// Checks if |path|, which must be non-null, is (looks like) a valid (relative)
// path. (On failure, returns |ERROR_INVALID_ARGUMENT| if |path| is not UTF-8,
// or |ERROR_PERMISSION_DENIED| if it is not relative.)
Error IsPathValid(const String& path);

// Checks if |whence| is a valid (known) |Whence| value. (On failure, returns
// |ERROR_UNIMPLEMENTED|.)
Error IsWhenceValid(Whence whence);

// Checks if |offset| is a valid file offset (from some point); this is
// implementation-dependent (typically checking if |offset| fits in an |off_t|).
// (On failure, returns |ERROR_OUT_OF_RANGE|.)
Error IsOffsetValid(int64_t offset);

// Conversion functions:

// Converts a standard errno value (|E...|) to an |Error| value.
Error ErrnoToError(int errno_value);

// Converts a |Whence| value to a standard whence value (|SEEK_...|).
int WhenceToStandardWhence(Whence whence);

// Converts a |Timespec| to a |struct timespec|. If |in| is null, |out->tv_nsec|
// is set to |UTIME_OMIT|.
Error TimespecToStandardTimespec(const Timespec* in, struct timespec* out);

// Converts a |TimespecOrNow| to a |struct timespec|. If |in| is null,
// |out->tv_nsec| is set to |UTIME_OMIT|; if |in->now| is set, |out->tv_nsec| is
// set to |UTIME_NOW|.
Error TimespecOrNowToStandardTimespec(const TimespecOrNow* in,
                                      struct timespec* out);

}  // namespace files
}  // namespace mojo

#endif  // SERVICES_FILES_UTIL_H_
