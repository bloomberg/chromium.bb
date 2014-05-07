// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EMBEDDER_PLATFORM_CHANNEL_UTILS_POSIX_H_
#define MOJO_EMBEDDER_PLATFORM_CHANNEL_UTILS_POSIX_H_

#include <stddef.h>
#include <sys/types.h>  // For |ssize_t|.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "mojo/embedder/platform_handle.h"
#include "mojo/system/system_impl_export.h"

struct iovec;  // Declared in <sys/uio.h>.

namespace mojo {
namespace embedder {

// The maximum number of handles that can be sent "at once" using
// |PlatformChannelSendHandles()|.
// TODO(vtl): This number is taken from ipc/file_descriptor_set_posix.h:
// |FileDescriptorSet::kMaxDescriptorsPerMessage|. Where does it come from?
const size_t kPlatformChannelMaxNumHandles = 7;

typedef std::vector<PlatformHandle> PlatformHandleVector;

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

// Sends |PlatformHandle|s (i.e., file descriptors) over the Unix domain socket
// (e.g., created using PlatformChannelPair|). (These will be sent in a single
// message having one null byte of data and one control message header with all
// the file descriptors.) All of the handles must be valid, and there must be at
// most |kPlatformChannelMaxNumHandles| (and at least one handle). Returns true
// on success, in which case it closes all the handles.
MOJO_SYSTEM_IMPL_EXPORT bool PlatformChannelSendHandles(PlatformHandle h,
                                                        PlatformHandle* handles,
                                                        size_t num_handles);

// Wrapper around |recvmsg()|, which will extract any attached file descriptors
// (in the control message) to |PlatformHandle|s. (This also handles |EINTR|.)
// If |*handles| is null and handles are received, a vector will be allocated;
// otherwise, any handles received will be appended to the existing vector.
MOJO_SYSTEM_IMPL_EXPORT ssize_t PlatformChannelRecvmsg(
    PlatformHandle h,
    void* buf,
    size_t num_bytes,
    scoped_ptr<PlatformHandleVector>* handles);

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EMBEDDER_PLATFORM_CHANNEL_UTILS_POSIX_H_
