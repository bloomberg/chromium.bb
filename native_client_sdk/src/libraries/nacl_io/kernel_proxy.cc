/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_io/kernel_proxy.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <iterator>
#include <string>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_dev.h"
#include "nacl_io/mount_html5fs.h"
#include "nacl_io/mount_http.h"
#include "nacl_io/mount_mem.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/mount_passthrough.h"
#include "nacl_io/osmman.h"
#include "nacl_io/osstat.h"
#include "nacl_io/path.h"
#include "nacl_io/pepper_interface.h"
#include "utils/auto_lock.h"
#include "utils/ref_object.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

// TODO(noelallen) : Grab/Redefine these in the kernel object once available.
#define USR_ID 1002
#define GRP_ID 1003



KernelProxy::KernelProxy()
   : dev_(0),
     ppapi_(NULL) {
}

KernelProxy::~KernelProxy() {
  delete ppapi_;
}

void KernelProxy::Init(PepperInterface* ppapi) {
  ppapi_ = ppapi;
  cwd_ = "/";
  dev_ = 1;

  factories_["memfs"] = MountMem::Create<MountMem>;
  factories_["dev"] = MountDev::Create<MountDev>;
  factories_["html5fs"] = MountHtml5Fs::Create<MountHtml5Fs>;
  factories_["httpfs"] = MountHttp::Create<MountHttp>;
  factories_["passthroughfs"] = MountPassthrough::Create<MountPassthrough>;

  // Create passthrough mount at root
  StringMap_t smap;
  mounts_["/"] = MountPassthrough::Create<MountPassthrough>(
      dev_++, smap, ppapi_);
  mounts_["/dev"] = MountDev::Create<MountDev>(dev_++, smap, ppapi_);

  // Open the first three in order to get STDIN, STDOUT, STDERR
  open("/dev/stdin", O_RDONLY);
  open("/dev/stdout", O_WRONLY);
  open("/dev/stderr", O_WRONLY);
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

  Mount* mount = handle->mount_;
  // Acquire the mount to ensure FreeFD doesn't prematurely destroy it.
  mount->Acquire();

  // FreeFD will release the handle/mount held by this fd.
  FreeFD(fd);

  // If this handle is the last reference to its node, releasing it will close
  // the node.
  ReleaseHandle(handle);

  // Finally, release the mount.
  mount->Release();

  return 0;
}

int KernelProxy::dup(int oldfd) {
  KernelHandle* handle = AcquireHandle(oldfd);
  if (NULL == handle) return -1;

  int newfd = AllocateFD(handle);
  ReleaseHandle(handle);

  return newfd;
}

int KernelProxy::dup2(int oldfd, int newfd) {
  // If it's the same file handle, just return
  if (oldfd == newfd) return newfd;

  KernelHandle* old_handle = AcquireHandle(oldfd);
  if (NULL == old_handle) return -1;

  FreeAndReassignFD(newfd, old_handle);
  ReleaseHandle(old_handle);
  return newfd;
}


char* KernelProxy::getcwd(char* buf, size_t size) {
  AutoLock lock(&process_lock_);
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
  int fd = open(path, O_RDONLY);
  if (-1 == fd) return -1;

  int ret = fstat(fd, buf);
  close(fd);
  return ret;
}

int KernelProxy::chdir(const char* path) {
  struct stat statbuf;
  if (stat(path, &statbuf) == -1)
    return -1;

  bool is_dir = (statbuf.st_mode & S_IFDIR) != 0;
  if (is_dir) {
    AutoLock lock(&process_lock_);
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
  std::string abs_targ;

  // Scope this lock to prevent holding both process and kernel locks
  {
    AutoLock lock(&process_lock_);
    abs_targ = GetAbsPathLocked(target).Join();
  }

  AutoLock lock(&kernel_lock_);
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

  Mount* mnt = factory->second(dev_++, smap, ppapi_);
  if (mnt) {
    mounts_[abs_targ] = mnt;
    return 0;
  }
  errno = EINVAL;
  return -1;
}

int KernelProxy::umount(const char *path) {
  Path abs_path;

  // Scope this lock to prevent holding both process and kernel locks
  {
    AutoLock lock(&process_lock_);
    abs_path = GetAbsPathLocked(path);
  }

  AutoLock lock(&kernel_lock_);
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

off_t KernelProxy::lseek(int fd, off_t offset, int whence) {
  KernelHandle* handle = AcquireHandle(fd);

  // check if fd is valid and handle exists
  if (NULL == handle) return -1;

  AutoLock lock(&handle->lock_);
  int ret = handle->Seek(offset, whence);

  ReleaseHandle(handle);
  return ret;
}

int KernelProxy::unlink(const char* path) {
  Path rel;
  Mount* mnt = AcquireMountAndPath(path, &rel);
  if (mnt == NULL) return -1;

  int val = mnt->Unlink(rel);
  ReleaseMount(mnt);
  return val;
}

int KernelProxy::remove(const char* path) {
  Path rel;
  Mount* mnt = AcquireMountAndPath(path, &rel);
  if (mnt == NULL) return -1;

  int val = mnt->Remove(rel);
  ReleaseMount(mnt);
  return val;
}

// TODO(noelallen): Needs implementation.
int KernelProxy::fchmod(int fd, int mode) {
  errno = EINVAL;
  return -1;
}

int KernelProxy::access(const char* path, int amode) {
  errno = EINVAL;
  return -1;
}

int KernelProxy::link(const char* oldpath, const char* newpath) {
  errno = EINVAL;
  return -1;
}

int KernelProxy::symlink(const char* oldpath, const char* newpath) {
  errno = EINVAL;
  return -1;
}

void* KernelProxy::mmap(void* addr, size_t length, int prot, int flags, int fd,
                        size_t offset) {
  // We shouldn't be getting anonymous mmaps here.
  assert((flags & MAP_ANONYMOUS) == 0);
  assert(fd != -1);

  KernelHandle* handle = AcquireHandle(fd);

  if (NULL == handle)
    return MAP_FAILED;

  void* new_addr;
  {
    AutoLock lock(&handle->lock_);
    new_addr = handle->node_->MMap(addr, length, prot, flags, offset);
    if (new_addr == MAP_FAILED) {
      ReleaseHandle(handle);
      return MAP_FAILED;
    }
  }

  // Don't release the KernelHandle, it is now owned by the MMapInfo.
  AutoLock lock(&process_lock_);
  mmap_info_list_.push_back(MMapInfo(new_addr, length, handle));

  return new_addr;
}

int KernelProxy::munmap(void* addr, size_t length) {
  if (addr == NULL || length == 0) {
    errno = EINVAL;
    return -1;
  }

  MMapInfoList_t unmap_list;
  {
    AutoLock lock(&process_lock_);
    int mmap_list_end = mmap_info_list_.size();
    void* addr_end = static_cast<char*>(addr) + length;

    for (int i = 0; i < mmap_list_end;) {
      const MMapInfo& mmap_info = mmap_info_list_[i];
      if (addr < static_cast<char*>(mmap_info.addr) + mmap_info.length &&
          mmap_info.addr < addr_end)
        // This memory area should be unmapped; swap it with the last entry in
        // our list.
        std::swap(mmap_info_list_[i], mmap_info_list_[--mmap_list_end]);
      else
        ++i;
    }

    int num_to_unmap =- mmap_info_list_.size() - mmap_list_end;
    if (!num_to_unmap) {
      // From the Linux mmap man page: "It is not an error if the indicated
      // range does not contain any mapped pages."
      return 0;
    }

    std::copy(mmap_info_list_.begin() + mmap_list_end, mmap_info_list_.end(),
              std::back_inserter(unmap_list));

    mmap_info_list_.resize(mmap_list_end);
  }

  // Unmap everything past the new end of the list.
  for (int i = 0; i < unmap_list.size(); ++i) {
    const MMapInfo& mmap_info = unmap_list[i];
    KernelHandle* handle = mmap_info.handle;
    assert(handle != NULL);

    // Ignore the results from individual munmaps.
    handle->node_->Munmap(mmap_info.addr, mmap_info.length);
    ReleaseHandle(handle);
  }

  return 0;
}

int KernelProxy::open_resource(const char* path) {
  Path rel;

  Mount* mnt = AcquireMountAndPath(path, &rel);
  if (mnt == NULL) return -1;

  MountNode* node = mnt->OpenResource(rel);
  if (node == NULL) {
    node = mnt->Open(rel, O_RDONLY);
    if (node == NULL) {
      ReleaseMount(mnt);
      return -1;
    }
  }

  // OpenResource failed, try Open().

  KernelHandle* handle = new KernelHandle(mnt, node, O_RDONLY);
  int fd = AllocateFD(handle);
  mnt->AcquireNode(node);

  ReleaseHandle(handle);
  ReleaseMount(mnt);

  return fd;
}
