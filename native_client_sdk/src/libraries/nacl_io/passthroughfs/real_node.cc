// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/passthroughfs/real_node.h"

#include <errno.h>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_wrap_real.h"

namespace nacl_io {
RealNode::RealNode(Filesystem* filesystem, int real_fd, bool close_on_destroy)
  : Node(filesystem),
    real_fd_(real_fd),
    close_on_destroy_(close_on_destroy)
{
}

void RealNode::Destroy() {
  if (close_on_destroy_)
    _real_close(real_fd_);
  real_fd_ = -1;
}

// Normal read/write operations on a file
Error RealNode::Read(const HandleAttr& attr,
                   void* buf,
                   size_t count,
                   int* out_bytes) {
  *out_bytes = 0;

  int64_t new_offset;
  int err = _real_lseek(real_fd_, attr.offs, 0, &new_offset);
  if (err && err != ESPIPE)
    return err;

  size_t nread;
  err = _real_read(real_fd_, buf, count, &nread);
  if (err)
    return err;

  *out_bytes = static_cast<int>(nread);
  return 0;
}

Error RealNode::Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  //nacl_io_log("Real::Write\n");
  int err;
  *out_bytes = 0;

  int64_t new_offset;
  err = _real_lseek(real_fd_, attr.offs, 0, &new_offset);
  if (err && err != ESPIPE)
    return err;

  size_t nwrote;
  err = _real_write(real_fd_, buf, count, &nwrote);
  if (err)
    return err;

  *out_bytes = static_cast<int>(nwrote);
  return 0;
}

Error RealNode::FTruncate(off_t size) {
  // TODO(binji): what to do here?
  return ENOSYS;
}

Error RealNode::GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes) {
  size_t nread;
  int err = _real_getdents(real_fd_, pdir, count, &nread);
  if (err)
    return err;
  return nread;
}

Error RealNode::GetStat(struct stat* stat) {
  int err = _real_fstat(real_fd_, stat);
  if (err)
    return err;
  return 0;
}

Error RealNode::Isatty() {
#ifdef __GLIBC__
  // isatty is not yet hooked up to the IRT interface under glibc.
  return ENOTTY;
#else
  int result = 0;
  int err = _real_isatty(real_fd_, &result);
  if (err)
    return err;
  return 0;
#endif
}

Error RealNode::MMap(void* addr,
                     size_t length,
                     int prot,
                     int flags,
                     size_t offset,
                     void** out_addr) {
  *out_addr = addr;
  int err = _real_mmap(out_addr, length, prot, flags, real_fd_, offset);
  if (err)
    return err;
  return 0;
}

}
