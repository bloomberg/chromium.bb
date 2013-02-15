/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_H_

#include <string>

#include "nacl_io/osstat.h"
#include "utils/ref_object.h"

struct dirent;
struct stat;
class Mount;

class MountNode : public RefObject {
 protected:
  explicit MountNode(Mount* mount);
  virtual ~MountNode();

 protected:
  // Initialize with node specific flags, in this case stat permissions.
  virtual bool Init(int flags);
  virtual void Destroy();

 public:
  // Normal OS operations on a node (file), can be called by the kernel
  // directly so it must lock and unlock appropriately.  These functions
  // must not be called by the mount.
  virtual int FSync();
  virtual int GetDents(size_t offs, struct dirent* pdir, size_t count);
  virtual int GetStat(struct stat* stat);
  virtual int Ioctl(int request, char* arg);
  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Truncate(size_t size);
  virtual int Write(size_t offs, const void* buf, size_t count);
  virtual void* MMap(void* addr, size_t length, int prot, int flags,
                     size_t offset);
  virtual int Munmap(void* addr, size_t length);

  virtual int GetLinks();
  virtual int GetMode();
  virtual int GetType();
  virtual size_t GetSize();
  virtual bool IsaDir();
  virtual bool IsaFile();
  virtual bool IsaTTY();

 protected:
  // Directory operations on the node are done by the Mount. The mount's lock
  // must be held while these calls are made.

  // Adds or removes a directory entry updating the link numbers and refcount
  virtual int AddChild(const std::string& name, MountNode *node);
  virtual int RemoveChild(const std::string& name);

  // Find a child and return it without updating the refcount
  virtual MountNode* FindChild(const std::string& name);
  virtual int ChildCount();

  // Update the link count
  virtual void Link();
  virtual void Unlink();

protected:
  struct stat stat_;
  Mount* mount_;

  friend class Mount;
  friend class MountDev;
  friend class MountHtml5Fs;
  friend class MountHttp;
  friend class MountMem;
  friend class MountNodeDir;
};

#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_H_
