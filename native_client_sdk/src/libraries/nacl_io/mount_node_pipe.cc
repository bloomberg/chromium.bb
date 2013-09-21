// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_node_pipe.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include "nacl_io/event_emitter_pipe.h"
#include "nacl_io/ioctl.h"

namespace {
  const size_t kDefaultPipeSize = 512 * 1024;
}

namespace nacl_io {

MountNodePipe::MountNodePipe(Mount* mnt)
    : MountNodeStream(mnt),
      pipe_(new EventEmitterPipe(kDefaultPipeSize)) {
}

EventEmitter* MountNodePipe::GetEventEmitter() {
  return pipe_.get();
}

Error MountNodePipe::Read(size_t offs,
                          void *buf,
                          size_t count,
                          int* out_bytes) {
  int ms = (GetMode() & O_NONBLOCK) ? 0 : read_timeout_;

  EventListenerLock wait(GetEventEmitter());
  Error err = wait.WaitOnEvent(POLLIN, ms);
  if (err)
    return err;

  *out_bytes = pipe_->Read_Locked(static_cast<char *>(buf), count);
  return 0;
}

Error MountNodePipe::Write(size_t offs,
                           const void *buf,
                           size_t count,
                           int* out_bytes) {
  int ms = (GetMode() & O_NONBLOCK) ? 0 : write_timeout_;

  EventListenerLock wait(GetEventEmitter());
  Error err = wait.WaitOnEvent(POLLOUT, ms);
  if (err)
    return err;

  *out_bytes = pipe_->Write_Locked(static_cast<const char *>(buf), count);
  return 0;
}

}  // namespace nacl_io

