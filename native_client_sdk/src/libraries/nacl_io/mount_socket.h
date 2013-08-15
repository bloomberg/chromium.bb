// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_SOCKET_H_
#define LIBRARIES_NACL_IO_MOUNT_SOCKET_H_

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include "nacl_io/mount.h"

namespace nacl_io {

class MountSocket : public Mount {
 protected:
  MountSocket();

 private:
  virtual Error Access(const Path& path, int a_mode);
  virtual Error Open(const Path& path,
                     int o_flags,
                     ScopedMountNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int permissions);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);

 private:
  friend class KernelProxy;
  DISALLOW_COPY_AND_ASSIGN(MountSocket);
};

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
#endif  // LIBRARIES_NACL_IO_MOUNT_SOCKET_H_
