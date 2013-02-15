/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_io/mount_passthrough.h"
#include <errno.h>
#include "nacl_io/kernel_wrap_real.h"

class MountNodePassthrough : public MountNode {
 public:
  explicit MountNodePassthrough(Mount* mount, int real_fd)
    : MountNode(mount),
      real_fd_(real_fd) {
  }

 protected:
  virtual bool Init(int flags) {
    return true;
  }

  virtual void Destroy() {
    if (real_fd_)
      _real_close(real_fd_);
    real_fd_ = 0;
  }

 public:
  // Normal read/write operations on a file
  virtual int Read(size_t offs, void* buf, size_t count) {
    off_t new_offset;
    int err = _real_lseek(real_fd_, offs, 0, &new_offset);
    if (err) {
      errno = err;
      return -1;
    }

    size_t nread;
    err = _real_read(real_fd_, buf, count, &nread);
    if (err) {
      errno = err;
      return -1;
    }

    return static_cast<int>(nread);
  }

  virtual int Write(size_t offs, const void* buf, size_t count) {
    off_t new_offset;
    int err = _real_lseek(real_fd_, offs, 0, &new_offset);
    if (err) {
      errno = err;
      return -1;
    }

    size_t nwrote;
    err = _real_write(real_fd_, buf, count, &nwrote);
    if (err) {
      errno = err;
      return -1;
    }

    return static_cast<int>(nwrote);
  }

  virtual int Truncate(size_t size) {
    // TODO(binji): what to do here?
    return -1;
  }

  virtual int GetDents(size_t offs, struct dirent* pdir, size_t count) {
    size_t nread;
    int err = _real_getdents(real_fd_, pdir, count, &nread);
    if (err) {
      errno = err;
      return -1;
    }

    return nread;
  }

  virtual int GetStat(struct stat* stat) {
    int err = _real_fstat(real_fd_, stat);
    if (err) {
      errno = err;
      return -1;
    }

    return 0;
  }

  void* MMap(void* addr, size_t length, int prot, int flags, size_t offset) {
    void* new_addr = addr;
    int err = _real_mmap(&new_addr, length, prot, flags, real_fd_, offset);
    if (err) {
      errno = err;
      return (void*)-1;
    }

    return new_addr;
  }

 private:
  friend class MountPassthrough;

  int real_fd_;
};

MountPassthrough::MountPassthrough() {
}

bool MountPassthrough::Init(int dev, StringMap_t& args,
                            PepperInterface* ppapi) {
  Mount::Init(dev, args, ppapi);
  return true;
}

void MountPassthrough::Destroy() {
}

MountNode *MountPassthrough::Open(const Path& path, int mode) {
  int real_fd;
  int err = _real_open(path.Join().c_str(), mode, 0666, &real_fd);
  if (err) {
    errno = err;
    return NULL;
  }

  MountNodePassthrough* node = new MountNodePassthrough(this, real_fd);
  return node;
}

MountNode *MountPassthrough::OpenResource(const Path& path) {
  int real_fd;
  int err = _real_open_resource(path.Join().c_str(), &real_fd);
  if (err) {
    errno = err;
    return NULL;
  }

  MountNodePassthrough* node = new MountNodePassthrough(this, real_fd);
  return node;
}

int MountPassthrough::Unlink(const Path& path) {
  // Not implemented by NaCl.
  errno = ENOSYS;
  return -1;
}

int MountPassthrough::Mkdir(const Path& path, int perm) {
  int err = _real_mkdir(path.Join().c_str(), perm);
  if (err) {
    errno = err;
    return -1;
  }

  return 0;
}

int MountPassthrough::Rmdir(const Path& path) {
  int err = _real_rmdir(path.Join().c_str());
  if (err) {
    errno = err;
    return -1;
  }

  return 0;
}

int MountPassthrough::Remove(const Path& path) {
  // Not implemented by NaCl.
  errno = ENOSYS;
  return -1;
}
