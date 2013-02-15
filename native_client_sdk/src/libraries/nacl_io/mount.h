/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_MOUNT_H_
#define LIBRARIES_NACL_IO_MOUNT_H_

#include <map>
#include <string>

#include "nacl_io/inode_pool.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/path.h"
#include "utils/macros.h"
#include "utils/ref_object.h"

class MountNode;
class PepperInterface;

typedef std::map<std::string, std::string> StringMap_t;


class Mount : public RefObject {
 protected:
  // The protected functions are only used internally and will not
  // acquire or release the mount's lock.
  Mount();
  virtual ~Mount();

  // Init must be called by the factory before the mount is used.
  // This function must assign a root node, or replace FindNode.
  // |ppapi| can be NULL. If so, this mount cannot make any pepper calls.
  virtual bool Init(int dev, StringMap_t& args, PepperInterface* ppapi);
  virtual void Destroy();

 public:
  template <class M>
  static Mount* Create(int dev, StringMap_t& args, PepperInterface* ppapi);

  PepperInterface* ppapi() { return ppapi_; }

  // All paths are expected to containing a leading "/"
  void AcquireNode(MountNode* node);
  void ReleaseNode(MountNode* node);

  // Open a node at |path| with the specified open flags. The resulting
  // MountNode is created with a ref count of 1.
  virtual MountNode *Open(const Path& path, int o_flags) = 0;

  // OpenResource is only used to read files from the NaCl NMF file. No mount
  // except MountPassthrough should implement it.
  virtual MountNode *OpenResource(const Path& path) { return NULL; }

  // Unlink, Mkdir, Rmdir will affect the both the RefCount
  // and the nlink number in the stat object.
  virtual int Unlink(const Path& path) = 0;
  virtual int Mkdir(const Path& path, int permissions) = 0;
  virtual int Rmdir(const Path& path) = 0;
  virtual int Remove(const Path& path) = 0;

  // Convert from R,W,R/W open flags to STAT permission flags
  static int OpenModeToPermission(int mode);

  void OnNodeCreated(MountNode* node) ;
  void OnNodeDestroyed(MountNode* node);

 protected:
  // Device number for the mount.
  int dev_;
  PepperInterface* ppapi_;  // Weak reference.
  INodePool inode_pool_;

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
Mount* Mount::Create(int dev, StringMap_t& args, PepperInterface* ppapi) {
  Mount* mnt = new M();
  if (mnt->Init(dev, args, ppapi) == false) {
    delete mnt;
    return NULL;
  }
  return mnt;
}

#endif  // LIBRARIES_NACL_IO_MOUNT_H_
