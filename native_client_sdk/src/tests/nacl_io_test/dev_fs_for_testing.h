// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_TEST_DEV_FS_FOR_TESTING_H_
#define LIBRARIES_NACL_IO_TEST_DEV_FS_FOR_TESTING_H_

#include "gmock/gmock.h"

#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/filesystem.h"

#define NULL_NODE ((Node*) NULL)

class DevFsForTesting : public nacl_io::DevFs {
 public:
  DevFsForTesting() {
    Init(nacl_io::FsInitArgs(1));
  }
  int num_nodes() { return (int) inode_pool_.size(); }
};

#endif  // LIBRARIES_NACL_IO_TEST_DEV_FS_FOR_TESTING_H_
