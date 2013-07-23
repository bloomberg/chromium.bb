// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_DIR_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_DIR_H_

#include <map>
#include <string>

#include "nacl_io/mount_node.h"

namespace nacl_io {

class MountDev;
class MountHtml5Fs;
class MountHttp;
class MountMem;
class MountNodeDir;

typedef sdk_util::ScopedRef<MountNodeDir> ScopedMountNodeDir;

class MountNodeDir : public MountNode {
 protected:
  explicit MountNodeDir(Mount* mnt);
  virtual ~MountNodeDir();

 public:
  typedef std::map<std::string, ScopedMountNode> MountNodeMap_t;

  virtual Error FTruncate(off_t size);
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);
  virtual Error Read(size_t offs, void *buf, size_t count, int* out_bytes);
  virtual Error Write(size_t offs, const void *buf,
                      size_t count, int* out_bytes);

  // Adds a finds or adds a directory entry as an INO, updating the refcount
  virtual Error AddChild(const std::string& name, const ScopedMountNode& node);
  virtual Error RemoveChild(const std::string& name);
  virtual Error FindChild(const std::string& name, ScopedMountNode* out_node);
  virtual int ChildCount();


protected:
   void ClearCache();
   void BuildCache();

private:
  struct dirent* cache_;
  MountNodeMap_t map_;

  friend class MountDev;
  friend class MountMem;
  friend class MountHttp;
  friend class MountHtml5Fs;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_DIR_H_
