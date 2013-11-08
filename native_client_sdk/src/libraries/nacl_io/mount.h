// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_H_
#define LIBRARIES_NACL_IO_MOUNT_H_

#include <map>
#include <string>

#include "nacl_io/error.h"
#include "nacl_io/inode_pool.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/path.h"

#include "sdk_util/macros.h"
#include "sdk_util/ref_object.h"
#include "sdk_util/scoped_ref.h"

namespace nacl_io {

class Mount;
class MountNode;
class PepperInterface;

typedef sdk_util::ScopedRef<Mount> ScopedMount;
typedef std::map<std::string, std::string> StringMap_t;

// NOTE: The KernelProxy is the only class that should be setting errno. All
// other classes should return Error (as defined by nacl_io/error.h).
class Mount : public sdk_util::RefObject {
 protected:
  // The protected functions are only used internally and will not
  // acquire or release the mount's lock.
  Mount();
  virtual ~Mount();

  // Init must be called by the factory before the mount is used.
  // This function must assign a root node, or replace FindNode.
  // |ppapi| can be NULL. If so, this mount cannot make any pepper calls.
  virtual Error Init(int dev, StringMap_t& args, PepperInterface* ppapi);
  virtual void Destroy();

 public:
  PepperInterface* ppapi() { return ppapi_; }

  // All paths in functions below are expected to containing a leading "/".

  // Test whether a file or directory at a given path can be accessed.
  // Returns 0 on success, or an appropriate errno value on failure.
  virtual Error Access(const Path& path, int a_mode) = 0;

  // Open a node at |path| with the specified open flags. The resulting
  // MountNode is created with a ref count of 1.
  // Assumes that |out_node| is non-NULL.
  virtual Error Open(const Path& path,
                     int open_flags,
                     ScopedMountNode* out_node) = 0;

  // OpenResource is only used to read files from the NaCl NMF file. No mount
  // except MountPassthrough should implement it.
  // Assumes that |out_node| is non-NULL.
  virtual Error OpenResource(const Path& path, ScopedMountNode* out_node);

  // Unlink, Mkdir, Rmdir will affect the both the RefCount
  // and the nlink number in the stat object.
  virtual Error Unlink(const Path& path) = 0;
  virtual Error Mkdir(const Path& path, int permissions) = 0;
  virtual Error Rmdir(const Path& path) = 0;
  virtual Error Remove(const Path& path) = 0;
  virtual Error Rename(const Path& path, const Path& newpath) = 0;

  // Assumes that |node| is non-NULL.
  void OnNodeCreated(MountNode* node);

  // Assumes that |node| is non-NULL.
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
  DISALLOW_COPY_AND_ASSIGN(Mount);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_H_
