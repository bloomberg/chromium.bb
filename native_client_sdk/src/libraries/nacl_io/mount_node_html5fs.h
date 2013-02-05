/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_MOUNT_HTML5FS_NODE_H_
#define LIBRARIES_NACL_IO_MOUNT_HTML5FS_NODE_H_

#include <ppapi/c/pp_resource.h>
#include "nacl_io/mount_node.h"

class MountHtml5Fs;

class MountNodeHtml5Fs : public MountNode {
 public:
  // Normal OS operations on a node (file), can be called by the kernel
  // directly so it must lock and unlock appropriately.  These functions
  // must not be called by the mount.
  virtual int FSync();
  virtual int GetDents(size_t offs, struct dirent* pdir, size_t count);
  virtual int GetStat(struct stat* stat);
  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Truncate(size_t size);
  virtual int Write(size_t offs, const void* buf, size_t count);

  virtual size_t GetSize();

 protected:
  MountNodeHtml5Fs(Mount* mount, PP_Resource fileref);

  // Init with standard open flags
  virtual bool Init(int o_mode);
  virtual void Destroy();

 private:
  PP_Resource fileref_resource_;
  PP_Resource fileio_resource_;

  friend class MountHtml5Fs;
};

#endif  // LIBRARIES_NACL_IO_MOUNT_HTML5FS_NODE_H_
