// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_passthrough.h"

#include <errno.h>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_wrap_real.h"

namespace nacl_io {

class MountNodePassthrough : public MountNode {
 public:
  explicit MountNodePassthrough(Mount* mount, int real_fd)
      : MountNode(mount), real_fd_(real_fd) {}

 protected:
  virtual Error Init(int flags) { return 0; }

  virtual void Destroy() {
    if (real_fd_)
      _real_close(real_fd_);
    real_fd_ = 0;
  }

 public:
  // Normal read/write operations on a file
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes) {
    *out_bytes = 0;

    off_t new_offset;
    int err = _real_lseek(real_fd_, attr.offs, 0, &new_offset);
    if (err)
      return err;

    size_t nread;
    err = _real_read(real_fd_, buf, count, &nread);
    if (err)
      return err;

    *out_bytes = static_cast<int>(nread);
    return 0;
  }

  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
    *out_bytes = 0;

    off_t new_offset;
    int err = _real_lseek(real_fd_, attr.offs, 0, &new_offset);
    if (err)
      return err;

    size_t nwrote;
    err = _real_write(real_fd_, buf, count, &nwrote);
    if (err)
      return err;

    *out_bytes = static_cast<int>(nwrote);
    return 0;
  }

  virtual Error FTruncate(off_t size) {
    // TODO(binji): what to do here?
    return ENOSYS;
  }

  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes) {
    size_t nread;
    int err = _real_getdents(real_fd_, pdir, count, &nread);
    if (err)
      return err;
    return nread;
  }

  virtual Error GetStat(struct stat* stat) {
    int err = _real_fstat(real_fd_, stat);
    if (err)
      return err;
    return 0;
  }

  Error MMap(void* addr,
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

 private:
  friend class MountPassthrough;

  int real_fd_;
};

MountPassthrough::MountPassthrough() {}

Error MountPassthrough::Init(int dev,
                             StringMap_t& args,
                             PepperInterface* ppapi) {
  return Mount::Init(dev, args, ppapi);
}

void MountPassthrough::Destroy() {}

Error MountPassthrough::Access(const Path& path, int a_mode) {
  // There is no underlying 'access' syscall in NaCl. It just returns ENOSYS.
  return ENOSYS;
}

Error MountPassthrough::Open(const Path& path,
                             int mode,
                             ScopedMountNode* out_node) {
  out_node->reset(NULL);
  int real_fd;
  int error = _real_open(path.Join().c_str(), mode, 0666, &real_fd);
  if (error)
    return error;

  out_node->reset(new MountNodePassthrough(this, real_fd));
  return 0;
}

Error MountPassthrough::OpenResource(const Path& path,
                                     ScopedMountNode* out_node) {
  int real_fd;
  out_node->reset(NULL);
  int error = _real_open_resource(path.Join().c_str(), &real_fd);
  if (error)
    return error;

  out_node->reset(new MountNodePassthrough(this, real_fd));
  return 0;
}

Error MountPassthrough::Unlink(const Path& path) {
  // Not implemented by NaCl.
  return ENOSYS;
}

Error MountPassthrough::Mkdir(const Path& path, int perm) {
  return _real_mkdir(path.Join().c_str(), perm);
}

Error MountPassthrough::Rmdir(const Path& path) {
  return _real_rmdir(path.Join().c_str());
}

Error MountPassthrough::Remove(const Path& path) {
  // Not implemented by NaCl.
  return ENOSYS;
}

Error MountPassthrough::Rename(const Path& path, const Path& newpath) {
  // Not implemented by NaCl.
  return ENOSYS;
}

}  // namespace nacl_io

