// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_PIPE_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_PIPE_H_

#include <map>
#include <string>

#include "nacl_io/event_emitter_pipe.h"
#include "nacl_io/mount_node_stream.h"

namespace nacl_io {

class MountNodePipe : public MountNodeStream {
 public:
  explicit MountNodePipe(Mount* mnt);

  virtual EventEmitter* GetEventEmitter();
  virtual Error Read(const HandleAttr& attr, void *buf, size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr, const void *buf,
                      size_t count, int* out_bytes);

 protected:
  ScopedEventEmitterPipe pipe_;

  friend class KernelProxy;
  friend class MountStream;
};


}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_PIPE_H_
