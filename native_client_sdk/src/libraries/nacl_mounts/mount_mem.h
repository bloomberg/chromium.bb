/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_MOUNT_MEM_H_
#define LIBRARIES_NACL_MOUNTS_MOUNT_MEM_H_

#include <stdint.h>
#include <map>
#include <string>

#include "mount.h"
#include "util/macros.h"
#include "util/SlotAllocator.h"

struct dirent;
struct stat;

class MountMemNode;

// Mount serves as the base mounting class that will be used by
// the mount manager (class MountManager).  The mount manager
// relies heavily on the GetNode method as a way of directing
// system calls that take a path as an argument.  The methods
// of this class are pure virtual.  BaseMount class contains
// stub implementations for these methods.  Feel free to use
// BaseMount if your mount does not implement all of these
// operations.
class MountMem : public Mount {
 protected:
  MountMem();
  virtual ~MountMem();

  // Init must be called by the factory before
  void Init();

  int MountMem::AddDirEntry(MountNode* node, MountNode* node, const char *name);

 public:
  // System calls that can be overridden by a mount implementation
  virtual int Creat(const std::string& path, int mode, struct stat *st);
  virtual int Mkdir(const std::string& path, int mode, struct stat *st);
  virtual int Unlink(const std::string& path);

  virtual int Rmdir(int node);
  virtual int Chmod(int node, int mode);
  virtual int Stat(int node, struct stat *buf);
  virtual int Fsync(int node);

  virtual int Getdents(int node, off_t offset, struct dirent *dirp,
                       unsigned int count);

  virtual ssize_t Read(int node, off_t offset,
                       void *buf, size_t count);
  virtual ssize_t Write(int node, off_t offset,
                        const void *buf, size_t count);
  virtual int Isatty(int node);

 private:
  pthread_mutex_t lock_;
  SlotAllocator<MountMemNode> inodes_;
  DISALLOW_COPY_AND_ASSIGN(MountMem);
};

#endif  // LIBRARIES_NACL_MOUNTS_MOUNT_MEM_H_

