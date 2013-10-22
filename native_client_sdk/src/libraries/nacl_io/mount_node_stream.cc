// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_node_stream.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include "nacl_io/ioctl.h"
#include "nacl_io/mount_stream.h"
#include "sdk_util/atomicops.h"


namespace nacl_io {

MountNodeStream::MountNodeStream(Mount* mnt)
    : MountNode(mnt),
      read_timeout_(-1),
      write_timeout_(-1),
      stream_state_flags_(0) {
}

Error MountNodeStream::Init(int open_flags) {
  MountNode::Init(open_flags);
  if (open_flags & O_NONBLOCK)
    SetStreamFlags(SSF_NON_BLOCK);

  return 0;
}

void MountNodeStream::SetStreamFlags(uint32_t bits) {
  sdk_util::AtomicOrFetch(&stream_state_flags_, bits);
}

void MountNodeStream::ClearStreamFlags(uint32_t bits) {
  sdk_util::AtomicAndFetch(&stream_state_flags_, ~bits);
}

uint32_t MountNodeStream::GetStreamFlags() {
  return stream_state_flags_;
}

bool MountNodeStream::TestStreamFlags(uint32_t bits) {
  return (stream_state_flags_ & bits) == bits;
}


void MountNodeStream::QueueInput() {}
void MountNodeStream::QueueOutput() {}

MountStream* MountNodeStream::mount_stream() {
  return static_cast<MountStream*>(mount_);
}

}  // namespace nacl_io
