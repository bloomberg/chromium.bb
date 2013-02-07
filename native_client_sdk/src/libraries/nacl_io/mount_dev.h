/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_MOUNT_DEV_H_
#define LIBRARIES_NACL_IO_MOUNT_DEV_H_

#include "nacl_io/mount.h"

class MountNode;

class MountDev : public Mount {
 public:
  virtual MountNode *Open(const Path& path, int mode);

  virtual int Unlink(const Path& path);
  virtual int Mkdir(const Path& path, int permissions);
  virtual int Rmdir(const Path& path);
  virtual int Remove(const Path& path);

 protected:
  MountDev();

  virtual bool Init(int dev, StringMap_t& args, PepperInterface* ppapi);
  virtual void Destroy();

 private:
  MountNode* root_;
  MountNode* null_node_;
  MountNode* zero_node_;
  MountNode* random_node_;
  MountNode* console0_node_;
  MountNode* console1_node_;
  MountNode* console2_node_;
  MountNode* console3_node_;
  MountNode* tty_node_;
  MountNode* stderr_node_;
  MountNode* stdin_node_;
  MountNode* stdout_node_;

  friend class Mount;
};

#endif  // LIBRARIES_NACL_IO_MOUNT_DEV_H_
