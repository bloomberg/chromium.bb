// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/filesystem.h"

#include <errno.h>
#include <fcntl.h>
#include <string>

#include "nacl_io/dir_node.h"
#include "nacl_io/node.h"
#include "nacl_io/osstat.h"
#include "nacl_io/path.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/ref_object.h"

#if defined(WIN32)
#include <windows.h>
#endif

namespace nacl_io {

Filesystem::Filesystem() : dev_(0) {}

Filesystem::~Filesystem() {}

Error Filesystem::Init(const FsInitArgs& args) {
  dev_ = args.dev;
  ppapi_ = args.ppapi;
  return 0;
}

void Filesystem::Destroy() {}

Error Filesystem::OpenResource(const Path& path, ScopedNode* out_node) {
  out_node->reset(NULL);
  return EINVAL;
}

void Filesystem::OnNodeCreated(Node* node) {
  node->stat_.st_ino = inode_pool_.Acquire();
  node->stat_.st_dev = dev_;
}

void Filesystem::OnNodeDestroyed(Node* node) {
  if (node->stat_.st_ino)
    inode_pool_.Release(node->stat_.st_ino);
}

}  // namespace nacl_io
