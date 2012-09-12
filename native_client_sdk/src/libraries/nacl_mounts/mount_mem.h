/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_MOUNT_MEM_H_
#define LIBRARIES_NACL_MOUNTS_MOUNT_MEM_H_

#include <map>
#include <string>

#include "nacl_mounts/mount.h"

class MountMem : public Mount {
 protected:
  MountMem();

  virtual bool Init(int dev, StringMap_t& args);
  virtual void Destroy();

  // The protected functions are only used internally and will not
  // acquire or release the mount's lock themselves.  The caller is
  // returned to use correct locking as needed.
  virtual MountNode *AllocateData(int mode);
  virtual MountNode *AllocatePath(int mode);
  virtual void ReleaseNode(MountNode *node);

  // Allocate or free an INODE number.
  int AllocateINO();
  void FreeINO(int ino);

  // Find a Node specified node optionally failing if type does not match.
  virtual MountNode* FindNode(const Path& path, int type = 0);

 public:
  typedef std::vector<ino_t> INOList_t;

  virtual MountNode *Open(const Path& path, int mode);
  virtual int Close(MountNode* node);
  virtual int Unlink(const Path& path);
  virtual int Mkdir(const Path& path, int perm);
  virtual int Rmdir(const Path& path);

private:
  MountNode* root_;
  INOList_t inos_;
  size_t max_ino_;

  friend class Mount;
  DISALLOW_COPY_AND_ASSIGN(MountMem);
};

#endif  // LIBRARIES_NACL_MOUNTS_MOUNT_MEM_H_
