// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_MEM_H_
#define LIBRARIES_NACL_IO_MOUNT_MEM_H_

#include "nacl_io/mount.h"
#include "nacl_io/typed_mount_factory.h"

namespace nacl_io {

class MountMem : public Mount {
 protected:
  MountMem();

  virtual Error Init(int dev, StringMap_t& args, PepperInterface* ppapi);

  // The protected functions are only used internally and will not
  // acquire or release the mount's lock themselves.  The caller is
  // required to use correct locking as needed.

  // Allocate or free an INODE number.
  int AllocateINO();
  void FreeINO(int ino);

  // Find a Node specified node optionally failing if type does not match.
  virtual Error FindNode(const Path& path, int type, ScopedMountNode* out_node);

 public:
  virtual Error Access(const Path& path, int a_mode);
  virtual Error Open(const Path& path, int mode, ScopedMountNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int perm);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

private:
  static const int REMOVE_DIR = 1;
  static const int REMOVE_FILE = 2;
  static const int REMOVE_ALL = REMOVE_DIR | REMOVE_FILE;

  Error RemoveInternal(const Path& path, int remove_type);

  ScopedMountNode root_;

  friend class TypedMountFactory<MountMem>;
  DISALLOW_COPY_AND_ASSIGN(MountMem);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_MEM_H_
