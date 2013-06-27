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
#include "sdk_util/auto_lock.h"
#include "sdk_util/ref_object.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

// TODO(noelallen) : Grab/Redefine these in the kernel object once available.
#define USR_ID 1002
#define GRP_ID 1003

KernelProxy::KernelProxy() : dev_(0), ppapi_(NULL) {}

KernelProxy::~KernelProxy() { delete ppapi_; }

void KernelProxy::Init(PepperInterface* ppapi) {
  ppapi_ = ppapi;
  cwd_ = "/";
  dev_ = 1;

  factories_["memfs"] = MountMem::Create<MountMem>;
  factories_["dev"] = MountDev::Create<MountDev>;
  factories_["html5fs"] = MountHtml5Fs::Create<MountHtml5Fs>;
  factories_["httpfs"] = MountHttp::Create<MountHttp>;
  factories_["passthroughfs"] = MountPassthrough::Create<MountPassthrough>;

  int result;
  result = mount("", "/", "passthroughfs", 0, NULL);
  assert(result == 0);

  result = mount("", "/dev", "dev", 0, NULL);
  assert(result == 0);

  // Open the first three in order to get STDIN, STDOUT, STDERR
  open("/dev/stdin", O_RDONLY);
  open("/dev/stdout", O_WRONLY);
  open("/dev/stderr", O_WRONLY);
}

int KernelProxy::open(const char* path, int oflags) {
  Path rel;

  ScopedMount mnt;
  Error error = AcquireMountAndPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  ScopedMountNode node;
  error = mnt->Open(rel, oflags, &node);
  if (error) {
    errno = error;
    return -1;
  }

  ScopedKernelHandle handle(new KernelHandle(mnt, node));
  error = handle->Init(oflags);
  if (error) {
    errno = error;
    return -1;
  }

  return AllocateFD(handle);
}


int KernelProxy::close(int fd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  // Remove the FD from the process open file descriptor map
  FreeFD(fd);
  return 0;
}

int KernelProxy::dup(int oldfd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(oldfd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  return AllocateFD(handle);
}

int KernelProxy::dup2(int oldfd, int newfd) {
  // If it's the same file handle, just return
  if (oldfd == newfd)
    return newfd;

  ScopedKernelHandle old_handle;
  Error error = AcquireHandle(oldfd, &old_handle);
  if (error) {
    errno = error;
    return -1;
  }

  FreeAndReassignFD(newfd, old_handle);
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

int KernelProxy::chmod(const char* path, mode_t mode) {
  int fd = KernelProxy::open(path, O_RDWR);
  if (-1 == fd)
    return -1;

  int result = fchmod(fd, mode);
  close(fd);
  return result;
}

int KernelProxy::mkdir(const char* path, mode_t mode) {
  ScopedMount mnt;
  Path rel;
  Error error = AcquireMountAndPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Mkdir(rel, mode);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::rmdir(const char* path) {
  ScopedMount mnt;
  Path rel;
  Error error = AcquireMountAndPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Rmdir(rel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::stat(const char* path, struct stat* buf) {
  int fd = open(path, O_RDONLY);
  if (-1 == fd)
    return -1;

  int result = fstat(fd, buf);
  close(fd);
  return result;
}

int KernelProxy::chdir(const char* path) {
  struct stat statbuf;
  if (stat(path, &statbuf) == -1)
    return -1;

  bool is_dir = (statbuf.st_mode & S_IFDIR) != 0;
  if (!is_dir) {
    errno = ENOTDIR;
    return -1;
  }

  AutoLock lock(&process_lock_);
  cwd_ = GetAbsPathLocked(path).Join();
  return 0;
}

int KernelProxy::mount(const char* source,
                       const char* target,
                       const char* filesystemtype,
                       unsigned long mountflags,
                       const void* data) {
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
    char* str = strdup(static_cast<const char*>(data));
    char* ptr = strtok(str, ",");
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

  Error error = factory->second(dev_++, smap, ppapi_, &mounts_[abs_targ]);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::umount(const char* path) {
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

  mounts_.erase(it);
  return 0;
}

ssize_t KernelProxy::read(int fd, void* buf, size_t nbytes) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  AutoLock lock(&handle->lock_);
  int cnt = 0;
  error = handle->node_->Read(handle->offs_, buf, nbytes, &cnt);
  if (error) {
    errno = error;
    return -1;
  }

  if (cnt > 0)
    handle->offs_ += cnt;

  return cnt;
}

ssize_t KernelProxy::write(int fd, const void* buf, size_t nbytes) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  AutoLock lock(&handle->lock_);
  int cnt = 0;
  error = handle->node_->Write(handle->offs_, buf, nbytes, &cnt);
  if (error) {
    errno = error;
    return -1;
  }

  if (cnt > 0)
    handle->offs_ += cnt;

  return cnt;
}

int KernelProxy::fstat(int fd, struct stat* buf) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node_->GetStat(buf);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::getdents(int fd, void* buf, unsigned int count) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  AutoLock lock(&handle->lock_);
  int cnt = 0;
  error = handle->node_
      ->GetDents(handle->offs_, static_cast<dirent*>(buf), count, &cnt);
  if (error)
    errno = error;

  if (cnt > 0)
    handle->offs_ += cnt;

  return cnt;
}

int KernelProxy::ftruncate(int fd, off_t length) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node_->FTruncate(length);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::fsync(int fd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node_->FSync();
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::isatty(int fd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node_->IsaTTY();
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

off_t KernelProxy::lseek(int fd, off_t offset, int whence) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  AutoLock lock(&handle->lock_);
  off_t new_offset;
  error = handle->Seek(offset, whence, &new_offset);
  if (error) {
    errno = error;
    return -1;
  }

  return new_offset;
}

int KernelProxy::unlink(const char* path) {
  ScopedMount mnt;
  Path rel;
  Error error = AcquireMountAndPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Unlink(rel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::remove(const char* path) {
  ScopedMount mnt;
  Path rel;
  Error error = AcquireMountAndPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Remove(rel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

// TODO(noelallen): Needs implementation.
int KernelProxy::fchmod(int fd, int mode) {
  errno = EINVAL;
  return -1;
}

int KernelProxy::access(const char* path, int amode) {
  Path rel;

  ScopedMount mnt;
  Error error = AcquireMountAndPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Access(rel, amode);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

// TODO(noelallen): Needs implementation.
int KernelProxy::link(const char* oldpath, const char* newpath) {
  errno = EINVAL;
  return -1;
}

int KernelProxy::symlink(const char* oldpath, const char* newpath) {
  errno = EINVAL;
  return -1;
}

void* KernelProxy::mmap(void* addr,
                        size_t length,
                        int prot,
                        int flags,
                        int fd,
                        size_t offset) {
  // We shouldn't be getting anonymous mmaps here.
  assert((flags & MAP_ANONYMOUS) == 0);
  assert(fd != -1);

  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return MAP_FAILED;
  }

  void* new_addr;
  AutoLock lock(&handle->lock_);
  error = handle->node_->MMap(addr, length, prot, flags, offset, &new_addr);
  if (error) {
    errno = error;
    return MAP_FAILED;
  }

  return new_addr;
}

int KernelProxy::munmap(void* addr, size_t length) {
  // NOTE: The comment below is from a previous discarded implementation that
  // tracks mmap'd regions. For simplicity, we no longer do this; because we
  // "snapshot" the contents of the file in mmap(), and don't support
  // write-back or updating the mapped region when the file is written, holding
  // on to the KernelHandle is pointless.
  //
  // If we ever do, these threading issues should be considered.

  //
  // WARNING: this function may be called by free().
  //
  // There is a potential deadlock scenario:
  // Thread 1: open() -> takes lock1 -> free() -> takes lock2
  // Thread 2: free() -> takes lock2 -> munmap() -> takes lock1
  //
  // Note that open() above could be any function that takes a lock that is
  // shared with munmap (this includes munmap!)
  //
  // To prevent this, we avoid taking locks in munmap() that are used by other
  // nacl_io functions that may call free. Specifically, we only take the
  // mmap_lock, which is only shared with mmap() above. There is still a
  // possibility of deadlock if mmap() or munmap() calls free(), so this is not
  // allowed.
  //
  // Unfortunately, munmap still needs to acquire other locks; see the call to
  // ReleaseHandle below which takes the process lock. This is safe as long as
  // this is never executed from free() -- we can be reasonably sure this is
  // true, because malloc only makes anonymous mmap() requests, and should only
  // be munmapping those allocations. We never add to mmap_info_list_ for
  // anonymous maps, so the unmap_list should always be empty when called from
  // free().
  return 0;
}

int KernelProxy::open_resource(const char* path) {
  ScopedMount mnt;
  Path rel;
  Error error = AcquireMountAndPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  ScopedMountNode node;
  error = mnt->OpenResource(rel, &node);
  if (error) {
    // OpenResource failed, try Open().
    error = mnt->Open(rel, O_RDONLY, &node);
    if (error) {
      errno = error;
      return -1;
    }
  }

  ScopedKernelHandle handle(new KernelHandle(mnt, node));
  error = handle->Init(O_RDONLY);
  if (error) {
    errno = error;
    return -1;
  }

  return AllocateFD(handle);
}

