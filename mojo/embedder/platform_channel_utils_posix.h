// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EMBEDDER_PLATFORM_CHANNEL_UTILS_POSIX_H_
#define MOJO_EMBEDDER_PLATFORM_CHANNEL_UTILS_POSIX_H_

#include <stddef.h>
#include <sys/types.h>  // For |ssize_t|.

#include "mojo/embedder/platform_handle.h"
#include "mojo/system/system_impl_export.h"

struct iovec;  // Declared in <sys/uio.h>.

namespace mojo {
namespace embedder {

// Use these to write to a socket created using |PlatformChannelPair| (or
// equivalent). These are like |write()| and |writev()|, but handle |EINTR| and
// never raise |SIGPIPE|. (Note: On Mac, the suppression of |SIGPIPE| is set up
// by |PlatformChannelPair|.)
MOJO_SYSTEM_IMPL_EXPORT ssize_t PlatformChannelWrite(PlatformHandle h,
                                                     const void* bytes,
                                                     size_t num_bytes);
MOJO_SYSTEM_IMPL_EXPORT ssize_t PlatformChannelWritev(PlatformHandle h,
                                                      struct iovec* iov,
                                                      size_t num_iov);

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EMBEDDER_PLATFORM_CHANNEL_UTILS_POSIX_H_
