// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/passthroughfs/passthrough_fs.h"

#include <errno.h>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_wrap_real.h"

namespace nacl_io {

class PassthroughFsNode : public Node {
 public:
  explicit PassthroughFsNode(Filesystem* filesystem, int real_fd)
      : Node(filesystem), real_fd_(real_fd) {}

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

    int64_t new_offset;
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

    int64_t new_offset;
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

  virtual Error Isatty() {
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
  friend class PassthroughFs;

  int real_fd_;
};

PassthroughFs::PassthroughFs() {
}

Error PassthroughFs::Init(const FsInitArgs& args) {
  return Filesystem::Init(args);
}

void PassthroughFs::Destroy() {
}

Error PassthroughFs::Access(const Path& path, int a_mode) {
  // There is no underlying 'access' syscall in NaCl. It just returns ENOSYS.
  return ENOSYS;
}

Error PassthroughFs::Open(const Path& path, int mode, ScopedNode* out_node) {
  out_node->reset(NULL);
  int real_fd;
  int error = _real_open(path.Join().c_str(), mode, 0666, &real_fd);
  if (error)
    return error;

  out_node->reset(new PassthroughFsNode(this, real_fd));
  return 0;
}

Error PassthroughFs::OpenResource(const Path& path, ScopedNode* out_node) {
  int real_fd;
  out_node->reset(NULL);
  int error = _real_open_resource(path.Join().c_str(), &real_fd);
  if (error)
    return error;

  out_node->reset(new PassthroughFsNode(this, real_fd));
  return 0;
}

Error PassthroughFs::Unlink(const Path& path) {
  // Not implemented by NaCl.
  return ENOSYS;
}

Error PassthroughFs::Mkdir(const Path& path, int perm) {
  return _real_mkdir(path.Join().c_str(), perm);
}

Error PassthroughFs::Rmdir(const Path& path) {
  return _real_rmdir(path.Join().c_str());
}

Error PassthroughFs::Remove(const Path& path) {
  // Not implemented by NaCl.
  return ENOSYS;
}

Error PassthroughFs::Rename(const Path& path, const Path& newpath) {
  // Not implemented by NaCl.
  return ENOSYS;
}

}  // namespace nacl_io
