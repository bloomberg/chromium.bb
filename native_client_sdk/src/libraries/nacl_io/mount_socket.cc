// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <errno.h>

#include "nacl_io/mount_node_socket.h"
#include "nacl_io/mount_socket.h"

namespace nacl_io {

MountSocket::MountSocket() {}

Error MountSocket::Access(const Path& path, int a_mode) { return EACCES; }
Error MountSocket::Open(const Path& path,
                        int o_flags,
                        ScopedMountNode* out_node) { return EACCES; }

Error MountSocket::Unlink(const Path& path) { return EACCES; }
Error MountSocket::Mkdir(const Path& path, int permissions) { return EACCES; }
Error MountSocket::Rmdir(const Path& path) { return EACCES; }
Error MountSocket::Remove(const Path& path) { return EACCES; }

}  // namespace nacl_io
#endif  // PROVIDES_SOCKET_API