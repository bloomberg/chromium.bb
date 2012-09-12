/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_mounts/kernel_proxy.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <string>

#include "nacl_mounts/kernel_handle.h"
#include "nacl_mounts/mount.h"
#include "nacl_mounts/mount_mem.h"
#include "nacl_mounts/mount_node.h"
#include "nacl_mounts/osstat.h"
// TODO(binji): implement MountURL
//#include "nacl_mounts/mount_url.h"
#include "nacl_mounts/path.h"
#include "utils/auto_lock.h"
#include "utils/ref_object.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

// TODO(noelallen) : Grab/Redefine these in the kernel object once available.
#define USR_ID 1002
#define GRP_ID 1003


KernelProxy::KernelProxy() : dev_(0) {}
KernelProxy::~KernelProxy() {}

void KernelProxy::Init() {
  cwd_ = "/";
  dev_ = 1;

  factories_["memfs"] = MountMem::Create<MountMem>;
  // TODO(binji): implement MountURL
  //factories_["urlfs"] = MountURL::Create;

  // Create memory mount at root
  StringMap_t smap;
  mounts_["/"] = MountMem::Create<MountMem>(dev_++, smap);
}


int KernelProxy::open(const char *path, int oflags) {
  Path rel;

  Mount* mnt = AcquireMountAndPath(path, &rel);
  if (mnt == NULL) return -1;

  MountNode* node = mnt->Open(rel, oflags);
  if (node == NULL) {
    ReleaseMount(mnt);
    return -1;
  }

  KernelHandle* handle = new KernelHandle(mnt, node, oflags);
  int fd = AllocateFD(handle);
  mnt->AcquireNode(node);

  ReleaseHandle(handle);
  ReleaseMount(mnt);
  return fd;
}

int KernelProxy::close(int fd) {
  KernelHandle* handle = AcquireHandle(fd);

  if (NULL == handle) return -1;
  handle->mount_->Close(handle->node_);

  FreeFD(fd);
  ReleaseHandle(handle);
  return 0;
}

int KernelProxy::dup(int oldfd) {
  KernelHandle* handle = AcquireHandle(oldfd);
  if (NULL == handle) return -1;

  int newfd = AllocateFD(handle);
  ReleaseHandle(handle);

  return newfd;
}


char* KernelProxy::getcwd(char* buf, size_t size) {
  AutoLock lock(&lock_);
  if (size <= 0) {
    errno = EINVAL;
    return NULL;
  }
  // If size is 0, allocate as much as we need.
  if (size == 0) {
    size = cwd_.size() + 1;
  }

  // Verify the buffer is large enough
  if (size <= cwd_.size()) {
    errno = ERANGE;
    return NULL;
  }

  // Allocate the buffer if needed
  if (buf == NULL) {
    buf = static_cast<char*>(malloc(size));
  }

  strcpy(buf, cwd_.c_str());
  return buf;
}

char* KernelProxy::getwd(char* buf) {
  if (NULL == buf) {
    errno = EFAULT;
    return NULL;
  }
  return getcwd(buf, MAXPATHLEN);
}

int KernelProxy::chmod(const char *path, mode_t mode) {
  int fd = KernelProxy::open(path, O_RDWR);
  if (-1 == fd) return -1;

  int ret = fchmod(fd, mode);
  close(fd);
  return ret;
}

int KernelProxy::mkdir(const char *path, mode_t mode) {
  Path rel;
  Mount* mnt = AcquireMountAndPath(path, &rel);
  if (mnt == NULL) return -1;

  int val = mnt->Mkdir(rel, mode);
  ReleaseMount(mnt);
  return val;
}

int KernelProxy::rmdir(const char *path) {
  Path rel;
  Mount* mnt = AcquireMountAndPath(path, &rel);
  if (mnt == NULL) return -1;

  int val = mnt->Rmdir(rel);
  ReleaseMount(mnt);
  return val;
}

int KernelProxy::stat(const char *path, struct stat *buf) {
  int fd = KernelProxy::open(path, O_RDONLY);
  if (-1 == fd) return -1;
  
  int ret = fstat(fd, buf);
  close(fd);
  return ret;
}

int KernelProxy::chdir(const char* path) {
  Path rel;
  Mount* mnt = AcquireMountAndPath(path, &rel);

  MountNode* node = mnt->Open(rel, O_RDONLY);
  if (NULL == node) {
    errno = EEXIST;
    ReleaseMount(mnt);
    return -1;
  }

  bool isDir = node->IsaDir();
  mnt->Close(node);
  ReleaseMount(mnt);

  if (isDir) {
    AutoLock lock(&lock_);
    cwd_ = GetAbsPathLocked(path).Join();
    return 0;
  }

  errno = ENOTDIR;
  return -1;
}

int KernelProxy::mount(const char *source, const char *target,
                       const char *filesystemtype, unsigned long mountflags,
                       const void *data) {
  // See if it's already mounted
  AutoLock lock(&lock_);

  std::string abs_targ = GetAbsPathLocked(target).Join();
  if (mounts_.find(abs_targ) != mounts_.end()) {
    errno = EBUSY;
    return -1;
  }

  // Find a factory of that type
  MountFactoryMap_t::iterator factory = factories_.find(filesystemtype);
  if (factory == factories_.end()) {
    errno = ENODEV;
    return -1;
  }

  StringMap_t smap;
  smap["SOURCE"] = source;
  smap["TARGET"] = abs_targ;

  if (data) {
    char* str = strdup(static_cast<const char *>(data));
    char* ptr = strtok(str,",");
    char* val;
    while (ptr != NULL) {
      val = strchr(ptr, '=');
      if (val) {
        *val = 0;
        smap[ptr] = val + 1;
      } else {
        smap[ptr] = "TRUE";
      }
      ptr = strtok(NULL, ",");
    }
    free(str);
  }

  Mount* mnt = factory->second(dev_++, smap);
  if (mnt) {
    mounts_[abs_targ] = mnt;
    return 0;
  }
  errno = EINVAL; 
  return -1;
}

int KernelProxy::umount(const char *path) {
  AutoLock lock(&lock_);

  Path abs_path = GetAbsPathLocked(path);
  MountMap_t::iterator it = mounts_.find(abs_path.Join());

  if (mounts_.end() == it) {
    errno = EINVAL;
    return -1;
  }

  if (it->second->RefCount() != 1) {
    errno = EBUSY;
    return -1;
  }

  it->second->Release();
  mounts_.erase(it);
  return 0;
}

ssize_t KernelProxy::read(int fd, void *buf, size_t nbytes) {
  KernelHandle* handle = AcquireHandle(fd);

  // check if fd is valid and handle exists
  if (NULL == handle) return -1;

  AutoLock lock(&handle->lock_);
  ssize_t cnt = handle->node_->Read(handle->offs_, buf, nbytes);
  if (cnt > 0) handle->offs_ += cnt;

  ReleaseHandle(handle);
  return cnt;
}

ssize_t KernelProxy::write(int fd, const void *buf, size_t nbytes) {
  KernelHandle* handle = AcquireHandle(fd);

  // check if fd is valid and handle exists
  if (NULL == handle) return -1;

  AutoLock lock(&handle->lock_);
  ssize_t cnt = handle->node_->Write(handle->offs_, buf, nbytes);
  if (cnt > 0) handle->offs_ += cnt;

  ReleaseHandle(handle);
  return cnt;
}

int KernelProxy::fstat(int fd, struct stat* buf) {
  KernelHandle* handle = AcquireHandle(fd);

  // check if fd is valid and handle exists
  if (NULL == handle) return -1;

  int ret = handle->node_->GetStat(buf);
  ReleaseHandle(handle);
  return ret;
}

int KernelProxy::getdents(int fd, void* buf, unsigned int count) {
  KernelHandle* handle = AcquireHandle(fd);

  // check if fd is valid and handle exists
  if (NULL == handle) return -1;

  AutoLock lock(&handle->lock_);
  int cnt = handle->node_->GetDents(handle->offs_,
      static_cast<dirent *>(buf), count);

  if (cnt > 0) handle->offs_ += cnt;

  ReleaseHandle(handle);
  return cnt;
}

int KernelProxy::fsync(int fd) {
  KernelHandle* handle = AcquireHandle(fd);

  // check if fd is valid and handle exists
  if (NULL == handle) return -1;
  int ret = handle->node_->FSync();

  ReleaseHandle(handle);
  return ret;
}

int KernelProxy::isatty(int fd) {
  KernelHandle* handle = AcquireHandle(fd);

  // check if fd is valid and handle exists
  if (NULL == handle) return -1;
  int ret = handle->node_->IsaTTY();

  ReleaseHandle(handle);
  return ret;
}

// TODO(noelallen): Needs implementation.
int KernelProxy::fchmod(int fd, int mode) {
  errno = EINVAL;
  return -1;
}
int KernelProxy::remove(const char* path) {
  errno = EINVAL;
  return -1;
}
int KernelProxy::unlink(const char* path) {
  errno = EINVAL;
  return -1;
}
int KernelProxy::access(const char*path, int amode) {
  errno = EINVAL;
  return -1;
}
off_t KernelProxy::lseek(int fd, off_t offset, int whence) {
  errno = EINVAL;
  return -1;
}
