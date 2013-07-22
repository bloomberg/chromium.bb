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
#include "nacl_io/typed_mount_factory.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/ref_object.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

namespace nacl_io {

KernelProxy::KernelProxy() : dev_(0), ppapi_(NULL) {
}

KernelProxy::~KernelProxy() {
  // Clean up the MountFactories.
  for (MountFactoryMap_t::iterator i = factories_.begin();
       i != factories_.end();
       ++i) {
    delete i->second;
  }

  delete ppapi_;
}

void KernelProxy::Init(PepperInterface* ppapi) {
  ppapi_ = ppapi;
  dev_ = 1;

  factories_["memfs"] = new TypedMountFactory<MountMem>;
  factories_["dev"] = new TypedMountFactory<MountDev>;
  factories_["html5fs"] = new TypedMountFactory<MountHtml5Fs>;
  factories_["httpfs"] = new TypedMountFactory<MountHttp>;
  factories_["passthroughfs"] = new TypedMountFactory<MountPassthrough>;

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

int KernelProxy::open_resource(const char* path) {
  ScopedMount mnt;
  Path rel;

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
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

int KernelProxy::open(const char* path, int oflags) {
  ScopedMount mnt;
  ScopedMountNode node;

  Error error = AcquireMountAndNode(path, oflags, &mnt, &node);
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

int KernelProxy::chdir(const char* path) {
  Error error = SetCWD(path);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

char* KernelProxy::getcwd(char* buf, size_t size) {
  std::string cwd = GetCWD();

  if (size <= 0) {
    errno = EINVAL;
    return NULL;
  }

  // If size is 0, allocate as much as we need.
  if (size == 0) {
    size = cwd.size() + 1;
  }

  // Verify the buffer is large enough
  if (size <= cwd.size()) {
    errno = ERANGE;
    return NULL;
  }

  // Allocate the buffer if needed
  if (buf == NULL) {
    buf = static_cast<char*>(malloc(size));
  }

  strcpy(buf, cwd.c_str());
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
  int fd = KernelProxy::open(path, O_RDONLY);
  if (-1 == fd)
    return -1;

  int result = fchmod(fd, mode);
  close(fd);
  return result;
}

int KernelProxy::chown(const char* path, uid_t owner, gid_t group) {
  return 0;
}

int KernelProxy::fchown(int fd, uid_t owner, gid_t group) {
  return 0;
}

int KernelProxy::lchown(const char* path, uid_t owner, gid_t group) {
  return 0;
}

int KernelProxy::utime(const char* filename, const struct utimbuf* times) {
  return 0;
}

int KernelProxy::mkdir(const char* path, mode_t mode) {
  ScopedMount mnt;
  Path rel;

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
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

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
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


int KernelProxy::mount(const char* source,
                       const char* target,
                       const char* filesystemtype,
                       unsigned long mountflags,
                       const void* data) {
  std::string abs_path = GetAbsParts(target).Join();

  // Find a factory of that type
  MountFactoryMap_t::iterator factory = factories_.find(filesystemtype);
  if (factory == factories_.end()) {
    errno = ENODEV;
    return -1;
  }

  // Create a map of settings
  StringMap_t smap;
  smap["SOURCE"] = source;
  smap["TARGET"] = abs_path;

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

  ScopedMount mnt;
  Error error = factory->second->CreateMount(dev_++, smap, ppapi_, &mnt);
  if (error) {
    errno = error;
    return -1;
  }

  error = AttachMountAtPath(mnt, abs_path);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::umount(const char* path) {
  Error error = DetachMountAtPath(path);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

ssize_t KernelProxy::read(int fd, void* buf, size_t nbytes) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int cnt = 0;
  error = handle->Read(buf, nbytes, &cnt);
  if (error) {
    errno = error;
    return -1;
  }

  return cnt;
}

ssize_t KernelProxy::write(int fd, const void* buf, size_t nbytes) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int cnt = 0;
  error = handle->Write(buf, nbytes, &cnt);
  if (error) {
    errno = error;
    return -1;
  }

  return cnt;
}

int KernelProxy::fstat(int fd, struct stat* buf) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->GetStat(buf);
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

  int cnt = 0;
  error = handle->GetDents(static_cast<dirent*>(buf), count, &cnt);
  if (error)
    errno = error;

  return cnt;
}

int KernelProxy::ftruncate(int fd, off_t length) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->FTruncate(length);
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

  error = handle->node()->FSync();
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

  error = handle->node()->IsaTTY();
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::ioctl(int d, int request, char* argp) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(d, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->Ioctl(request, argp);
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

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
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

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
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
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::access(const char* path, int amode) {
  ScopedMount mnt;
  Path rel;

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
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
  error = handle->node()->MMap(addr, length, prot, flags, offset, &new_addr);
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

}  // namespace nacl_io

