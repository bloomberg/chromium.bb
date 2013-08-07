// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_H_

#include <string>

#include "nacl_io/error.h"
#include "nacl_io/event_listener.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/osstat.h"
#include "nacl_io/ostermios.h"

#include "sdk_util/ref_object.h"
#include "sdk_util/scoped_ref.h"
#include "sdk_util/simple_lock.h"

namespace nacl_io {

class Mount;
class MountNode;

typedef sdk_util::ScopedRef<MountNode> ScopedMountNode;

// NOTE: The KernelProxy is the only class that should be setting errno. All
// other classes should return Error (as defined by nacl_io/error.h).
class MountNode : public EventListener {
 protected:
  explicit MountNode(Mount* mount);
  virtual ~MountNode();

 protected:
  // Initialize with node specific flags, in this case stat permissions.
  virtual Error Init(int flags);
  virtual void Destroy();

 public:
   // Declared in EventEmitter. defaults to signalled.
   virtual uint32_t GetEventStatus();

  // Normal OS operations on a node (file), can be called by the kernel
  // directly so it must lock and unlock appropriately.  These functions
  // must not be called by the mount.
  virtual Error FSync();
  // It is expected that the derived MountNode will fill with 0 when growing
  // the file.
  virtual Error FTruncate(off_t length);
  // Assume that |out_bytes| is non-NULL.
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);
  // Assume that |stat| is non-NULL.
  virtual Error GetStat(struct stat* stat);
  // Assume that |arg| is non-NULL.
  virtual Error Ioctl(int request, char* arg);
  // Assume that |buf| and |out_bytes| are non-NULL.
  virtual Error Read(size_t offs, void* buf, size_t count, int* out_bytes);
  // Assume that |buf| and |out_bytes| are non-NULL.
  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
  // Assume that |addr| and |out_addr| are non-NULL.
  virtual Error MMap(void* addr,
                     size_t length,
                     int prot,
                     int flags,
                     size_t offset,
                     void** out_addr);
  virtual Error Tcflush(int queue_selector);
  virtual Error Tcgetattr(struct termios* termios_p);
  virtual Error Tcsetattr(int optional_actions,
                          const struct termios *termios_p);

  virtual int GetLinks();
  virtual int GetMode();
  virtual int GetType();
  // Assume that |out_size| is non-NULL.
  virtual Error GetSize(size_t* out_size);
  virtual bool IsaDir();
  virtual bool IsaFile();
  virtual bool IsaTTY();


  // Number of children for this node (directory)
  virtual int ChildCount();

 protected:
  // Directory operations on the node are done by the Mount. The mount's lock
  // must be held while these calls are made.

  // Adds or removes a directory entry updating the link numbers and refcount
  // Assumes that |node| is non-NULL.
  virtual Error AddChild(const std::string& name, const ScopedMountNode& node);
  virtual Error RemoveChild(const std::string& name);

  // Find a child and return it without updating the refcount
  // Assumes that |out_node| is non-NULL.
  virtual Error FindChild(const std::string& name, ScopedMountNode* out_node);

  // Update the link count
  virtual void Link();
  virtual void Unlink();

 protected:
  struct stat stat_;
  sdk_util::SimpleLock node_lock_;

  // We use a pointer directly to avoid cycles in the ref count.
  // TODO(noelallen) We should change this so it's unnecessary for the node
  // to track it's parent.  When a node is unlinked, the mount should do
  // any cleanup it needs.
  Mount* mount_;

  friend class Mount;
  friend class MountDev;
  friend class MountHtml5Fs;
  friend class MountHttp;
  friend class MountMem;
  friend class MountNodeDir;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_H_
