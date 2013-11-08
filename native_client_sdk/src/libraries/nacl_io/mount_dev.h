// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_DEV_H_
#define LIBRARIES_NACL_IO_MOUNT_DEV_H_

#include "nacl_io/mount.h"
#include "nacl_io/typed_mount_factory.h"

namespace nacl_io {

class MountNode;

class MountDev : public Mount {
 public:
  virtual Error Access(const Path& path, int a_mode);
  virtual Error Open(const Path& path,
                     int open_flags,
                     ScopedMountNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int permissions);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

 protected:
  MountDev();

  virtual Error Init(int dev, StringMap_t& args, PepperInterface* ppapi);

 private:
  ScopedMountNode root_;

  friend class TypedMountFactory<MountDev>;
  DISALLOW_COPY_AND_ASSIGN(MountDev);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_DEV_H_
