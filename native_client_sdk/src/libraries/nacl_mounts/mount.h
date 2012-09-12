/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_MOUNT_H_
#define LIBRARIES_NACL_MOUNTS_MOUNT_H_

#include <map>
#include <string>

#include "nacl_mounts/mount_node.h"
#include "nacl_mounts/path.h"
#include "utils/macros.h"
#include "utils/ref_object.h"

class MountNode;
class MountManager;

typedef std::map<std::string, std::string> StringMap_t;


// Mount serves as the base mounting class that will be used by
// the mount manager (class MountManager).  The mount manager
// relies heavily on the GetNode method as a way of directing
// system calls that take a path as an argument.  The methods
// of this class are pure virtual.  BaseMount class contains
// stub implementations for these methods.  Feel free to use
// BaseMount if your mount does not implement all of these
// operations.
class Mount : public RefObject {
 protected:
  // The protected functions are only used internally and will not
  // acquire or release the mount's lock.
  Mount();
  virtual ~Mount();

  // Init must be called by the factory before the mount is used.
  // This function must assign a root node, or replace FindNode.
  virtual bool Init(int dev, StringMap_t& args);

  // Destroy is called when the reference count reaches zero,
  // just before the destructor is called.
  virtual void Destroy();

 public:
  template <class M>
  static Mount* Create(int dev, StringMap_t& args);

  // All paths are expected to containing a leading "/"
  virtual void AcquireNode(MountNode* node);
  virtual void ReleaseNode(MountNode* node);

  // Open and Close will affect the RefCount on the node, so
  // there must be a call to close for each call to open.
  virtual MountNode *Open(const Path& path, int mode) = 0;
  virtual int Close(MountNode* node) = 0;

  // Unlink, Mkdir, Rmdir will affect the both the RefCount
  // and the nlink number in the stat object.
  virtual int Unlink(const Path& path) = 0;
  virtual int Mkdir(const Path& path, int permissions) = 0;
  virtual int Rmdir(const Path& path) = 0;

  // Convert from R,W,R/W open flags to STAT permission flags
  static int OpenModeToPermission(int mode);

 protected:
  // Device number for the mount.
  int dev_;

 private:
  // May only be called by the KernelProxy when the Kernel's
  // lock is held, so we make it private.
  friend class KernelObject;
  friend class KernelProxy;
  void Acquire() { RefObject::Acquire(); }
  bool Release() { return RefObject::Release(); }

  DISALLOW_COPY_AND_ASSIGN(Mount);
};


template <class M>
/*static*/
Mount* Mount::Create(int dev, StringMap_t& args) {
  Mount* mnt = new M();
  if (mnt->Init(dev, args) == false) {
    delete mnt;
    return NULL;
  }
  return mnt;
}


#endif  // LIBRARIES_NACL_MOUNTS_MOUNT_H_
