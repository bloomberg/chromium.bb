// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount.h"

#include <errno.h>
#include <fcntl.h>
#include <string>

#include "nacl_io/mount_node.h"
#include "nacl_io/mount_node_dir.h"
#include "nacl_io/mount_node_mem.h"
#include "nacl_io/osstat.h"
#include "nacl_io/path.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/ref_object.h"

#if defined(WIN32)
#include <windows.h>
#endif

namespace nacl_io {

Mount::Mount() : dev_(0) {}

Mount::~Mount() {}

Error Mount::Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
  dev_ = dev;
  ppapi_ = ppapi;
  return 0;
}

void Mount::Destroy() {}

Error Mount::OpenResource(const Path& path, ScopedMountNode* out_node) {
  out_node->reset(NULL);
  return EINVAL;
}

void Mount::OnNodeCreated(MountNode* node) {
  node->stat_.st_ino = inode_pool_.Acquire();
  node->stat_.st_dev = dev_;
}

void Mount::OnNodeDestroyed(MountNode* node) {
  if (node->stat_.st_ino)
    inode_pool_.Release(node->stat_.st_ino);
}

}  // namespace nacl_io

