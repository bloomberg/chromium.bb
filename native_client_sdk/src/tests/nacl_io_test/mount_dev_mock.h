// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_TEST_MOUNT_NODE_MOCK_H_
#define LIBRARIES_NACL_IO_TEST_MOUNT_NODE_MOCK_H_

#include "gmock/gmock.h"

#include "nacl_io/mount.h"
#include "nacl_io/mount_dev.h"

#define NULL_NODE ((MountNode*) NULL)

class MountDevMock : public nacl_io::MountDev {
 public:
  MountDevMock() {
    nacl_io::StringMap_t map;
    Init(1, map, NULL);
  }
  int num_nodes() { return (int) inode_pool_.size(); }
};

#endif  // LIBRARIES_NACL_IO_TEST_MOUNT_NODE_MOCK_H_
