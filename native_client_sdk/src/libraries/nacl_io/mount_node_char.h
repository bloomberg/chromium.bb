// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_CHAR_H
#define LIBRARIES_NACL_IO_MOUNT_NODE_CHAR_H

#include "nacl_io/mount_node.h"

namespace nacl_io {

class MountNodeCharDevice : public MountNode {
 public:
  explicit MountNodeCharDevice(Mount* mount) : MountNode(mount) {
    SetType(S_IFCHR);
  }
};

}

#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_CHAR_H
