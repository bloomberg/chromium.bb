// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_HTML5FS_NODE_H_
#define LIBRARIES_NACL_IO_MOUNT_HTML5FS_NODE_H_

#include <ppapi/c/pp_resource.h>
#include "nacl_io/mount_node.h"

namespace nacl_io {

class MountHtml5Fs;

class MountNodeHtml5Fs : public MountNode {
 public:
  // Normal OS operations on a node (file), can be called by the kernel
  // directly so it must lock and unlock appropriately.  These functions
  // must not be called by the mount.
  virtual Error FSync();
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);
  virtual Error GetStat(struct stat* stat);
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error FTruncate(off_t size);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

  virtual int GetType();
  virtual Error GetSize(size_t *out_size);
  virtual bool IsaDir();
  virtual bool IsaFile();

 protected:
  MountNodeHtml5Fs(Mount* mount, PP_Resource fileref);

  // Init with standard open flags
  virtual Error Init(int open_flags);
  virtual void Destroy();

 private:
  PP_Resource fileref_resource_;
  PP_Resource fileio_resource_;  // 0 if the file is a directory.

  friend class MountHtml5Fs;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_HTML5FS_NODE_H_
