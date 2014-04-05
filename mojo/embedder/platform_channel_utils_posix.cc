// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/embedder/platform_channel_utils_posix.h"

#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"

namespace mojo {
namespace embedder {

// On Linux, |SIGPIPE| is suppressed by passing |MSG_NOSIGNAL| to
// |send()|/|sendmsg()|. (There is no way of suppressing |SIGPIPE| on
// |write()|/|writev().) On Mac, |SIGPIPE| is suppressed by setting the
// |SO_NOSIGPIPE| option on the socket.
//
// Performance notes:
//  - On Linux, we have to use |send()|/|sendmsg()| rather than
//    |write()|/|writev()| in order to suppress |SIGPIPE|. This is okay, since
//    |send()| is (slightly) faster than |write()| (!), while |sendmsg()| is
//    quite comparable to |writev()|.
//  - On Mac, we may use |write()|/|writev()|. Here, |write()| is considerably
//    faster than |send()|, whereas |sendmsg()| is quite comparable to
//    |writev()|.
//  - On both platforms, an appropriate |sendmsg()|/|writev()| is considerably
//    faster than two |send()|s/|write()|s.
//  - Relative numbers (minimum real times from 10 runs) for one |write()| of
//    1032 bytes, one |send()| of 1032 bytes, one |writev()| of 32+1000 bytes,
//    one |sendmsg()| of 32+1000 bytes, two |write()|s of 32 and 1000 bytes, two
//    |send()|s of 32 and 1000 bytes:
//    - Linux: 0.81 s, 0.77 s, 0.87 s, 0.89 s, 1.31 s, 1.22 s
//    - Mac: 2.21 s, 2.91 s, 2.98 s, 3.08 s, 3.59 s, 4.74 s

ssize_t PlatformChannelWrite(PlatformHandle h,
                             const void* bytes,
                             size_t num_bytes) {
#if defined(OS_MACOSX)
  return HANDLE_EINTR(write(h.fd, bytes, num_bytes));
#else
  return send(h.fd, bytes, num_bytes, MSG_NOSIGNAL);
#endif
}

ssize_t PlatformChannelWritev(PlatformHandle h,
                              struct iovec* iov,
                              size_t num_iov) {
#if defined(OS_MACOSX)
  return HANDLE_EINTR(writev(h.fd, iov, static_cast<int>(num_iov)));
#else
  struct msghdr msg = {};
  msg.msg_iov = iov;
  msg.msg_iovlen = num_iov;
  return HANDLE_EINTR(sendmsg(h.fd, &msg, MSG_NOSIGNAL));
#endif
}

}  // namespace embedder
}  // namespace mojo
