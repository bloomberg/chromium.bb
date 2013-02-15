/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_MOUNT_PASSTHROUGH_H_
#define LIBRARIES_NACL_IO_MOUNT_PASSTHROUGH_H_

#include "nacl_io/mount.h"

class MountPassthrough : public Mount {
 protected:
  MountPassthrough();

  virtual bool Init(int dev, StringMap_t& args, PepperInterface* ppapi);
  virtual void Destroy();

 public:
  virtual MountNode *Open(const Path& path, int mode);
  virtual MountNode *OpenResource(const Path& path);
  virtual int Unlink(const Path& path);
  virtual int Mkdir(const Path& path, int perm);
  virtual int Rmdir(const Path& path);
  virtual int Remove(const Path& path);

private:
  friend class Mount;
  DISALLOW_COPY_AND_ASSIGN(MountPassthrough);
};

#endif  // LIBRARIES_NACL_IO_MOUNT_PASSTHROUGH_H_
