// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_DEVFS_JSPIPE_NODE_H_
#define LIBRARIES_NACL_IO_DEVFS_JSPIPE_NODE_H_

#include "nacl_io/pipe/pipe_node.h"

#include <string>

namespace nacl_io {

class JSPipeNode : public PipeNode {
 public:
  explicit JSPipeNode(Filesystem* filesystem) : PipeNode(filesystem) {}

  virtual Error VIoctl(int request, va_list args);

  // Writes go directly to PostMessage, reads come from a pipe
  // that gets populated by incoming messages
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
  std::string name_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_DEVFS_JSPIPE_NODE_H_
