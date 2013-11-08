// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_PASSTHROUGH_H_
#define LIBRARIES_NACL_IO_MOUNT_PASSTHROUGH_H_

#include "nacl_io/mount.h"
#include "nacl_io/typed_mount_factory.h"

namespace nacl_io {

class MountPassthrough : public Mount {
 protected:
  MountPassthrough();

  virtual Error Init(int dev, StringMap_t& args, PepperInterface* ppapi);
  virtual void Destroy();

 public:
  virtual Error Access(const Path& path, int a_mode);
  virtual Error Open(const Path& path, int mode, ScopedMountNode* out_node);
  virtual Error OpenResource(const Path& path, ScopedMountNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int perm);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

private:
 friend class TypedMountFactory<MountPassthrough>;
 DISALLOW_COPY_AND_ASSIGN(MountPassthrough);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_PASSTHROUGH_H_
