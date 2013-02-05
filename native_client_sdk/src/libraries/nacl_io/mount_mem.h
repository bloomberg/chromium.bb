/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_MOUNT_MEM_H_
#define LIBRARIES_NACL_IO_MOUNT_MEM_H_

#include <map>
#include <string>

#include "nacl_io/mount.h"

class MountMem : public Mount {
 protected:
  MountMem();

  virtual bool Init(int dev, StringMap_t& args, PepperInterface* ppapi);
  virtual void Destroy();

  // The protected functions are only used internally and will not
  // acquire or release the mount's lock themselves.  The caller is
  // required to use correct locking as needed.
  MountNode *AllocateData(int mode);
  MountNode *AllocatePath(int mode);

  // Allocate or free an INODE number.
  int AllocateINO();
  void FreeINO(int ino);

  // Find a Node specified node optionally failing if type does not match.
  virtual MountNode* FindNode(const Path& path, int type = 0);

 public:
  virtual MountNode *Open(const Path& path, int mode);
  virtual int Unlink(const Path& path);
  virtual int Mkdir(const Path& path, int perm);
  virtual int Rmdir(const Path& path);
  virtual int Remove(const Path& path);

private:
  static const int REMOVE_DIR = 1;
  static const int REMOVE_FILE = 2;
  static const int REMOVE_ALL = REMOVE_DIR | REMOVE_FILE;

  int RemoveInternal(const Path& path, int remove_type);

  MountNode* root_;
  size_t max_ino_;

  friend class Mount;
  DISALLOW_COPY_AND_ASSIGN(MountMem);
};

#endif  // LIBRARIES_NACL_IO_MOUNT_MEM_H_
