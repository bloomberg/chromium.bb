/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>

#include "auto_lock.h"
#include "ref_object.h"

#include "nacl_mounts/mount.h"
#include "nacl_mounts/mount_node.h"
#include "nacl_mounts/mount_node_dir.h"
#include "nacl_mounts/mount_node_mem.h"
#include "nacl_mounts/path.h"


Mount::Mount()
    : dev_(0) {
}

Mount::~Mount() {}

bool Mount::Init(int dev, StringMap_t& args) {
  dev_ = dev;
  return true;
}

void Mount::Destroy() {}

void Mount::AcquireNode(MountNode* node) {
  AutoLock lock(&lock_);
  node->Acquire();
}

void Mount::ReleaseNode(MountNode* node) {
  AutoLock lock(&lock_);
  node->Release();
}

int Mount::OpenModeToPermission(int mode) {
  int out;
  switch (mode & 3) {
    case O_RDONLY: out = S_IREAD;
    case O_WRONLY: out = S_IWRITE;
    case O_RDWR: out = S_IREAD | S_IWRITE;
  }
  return out;
}


