/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <errno.h>
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/real_pepper_interface.h"


static KernelProxy* s_kp;

void ki_init(void* kp) {
  ki_init_ppapi(kp, 0, NULL);
}

void ki_init_ppapi(void* kp,
                   PP_Instance instance,
                   PPB_GetInterface get_browser_interface) {
  kernel_wrap_init();

  if (kp == NULL) kp = new KernelProxy();
  s_kp = static_cast<KernelProxy*>(kp);

  PepperInterface* ppapi = NULL;
  if (instance && get_browser_interface)
    ppapi = new RealPepperInterface(instance, get_browser_interface);

  s_kp->Init(ppapi);
}

int ki_is_initialized() {
  return s_kp != NULL;
}

void ki_uninit() {
  s_kp = NULL;
}

int ki_chdir(const char* path) {
  return s_kp->chdir(path);
}

char* ki_getcwd(char* buf, size_t size) {
  // gtest uses getcwd in a static initializer. If we haven't initialized the
  // kernel-intercept yet, just return ".".
  if (!ki_is_initialized()) {
    if (size < 2) {
      errno = ERANGE;
      return NULL;
    }
    buf[0] = '.';
    buf[1] = 0;
    return buf;
  }
  return s_kp->getcwd(buf, size);
}

char* ki_getwd(char* buf) {
  return s_kp->getwd(buf);
}

int ki_dup(int oldfd) {
  return s_kp->dup(oldfd);
}

int ki_dup2(int oldfd, int newfd) {
  return s_kp->dup2(oldfd, newfd);
}

int ki_chmod(const char *path, mode_t mode) {
  return s_kp->chmod(path, mode);
}

int ki_stat(const char *path, struct stat *buf) {
  return s_kp->stat(path, buf);
}

int ki_mkdir(const char *path, mode_t mode) {
  return s_kp->mkdir(path, mode);
}

int ki_rmdir(const char *path) {
  return s_kp->rmdir(path);
}

int ki_mount(const char *source, const char *target, const char *filesystemtype,
             unsigned long mountflags, const void *data) {
  return s_kp->mount(source, target, filesystemtype, mountflags, data);
}

int ki_umount(const char *path) {
  return s_kp->umount(path);
}

int ki_open(const char *path, int oflag) {
  return s_kp->open(path, oflag);
}

ssize_t ki_read(int fd, void *buf, size_t nbyte) {
  return s_kp->read(fd, buf, nbyte);
}

ssize_t ki_write(int fd, const void *buf, size_t nbyte) {
  return s_kp->write(fd, buf, nbyte);
}

int ki_fstat(int fd, struct stat *buf){
  return s_kp->fstat(fd, buf);
}

int ki_getdents(int fd, void *buf, unsigned int count) {
  return s_kp->getdents(fd, buf, count);
}

int ki_fsync(int fd) {
  return s_kp->fsync(fd);
}

int ki_isatty(int fd) {
  if (!ki_is_initialized())
    return 0;
  return s_kp->isatty(fd);
}

int ki_close(int fd) {
  if (!ki_is_initialized())
    return 0;
  return s_kp->close(fd);
}

off_t ki_lseek(int fd, off_t offset, int whence) {
  return s_kp->lseek(fd, offset, whence);
}

int ki_remove(const char* path) {
  return s_kp->remove(path);
}

int ki_unlink(const char* path) {
  return s_kp->unlink(path);
}

int ki_access(const char* path, int amode) {
  return s_kp->access(path, amode);
}

int ki_link(const char* oldpath, const char* newpath) {
  return s_kp->link(oldpath, newpath);
}

int ki_symlink(const char* oldpath, const char* newpath) {
  return s_kp->symlink(oldpath, newpath);
}

void* ki_mmap(void* addr, size_t length, int prot, int flags, int fd,
              off_t offset) {
  return s_kp->mmap(addr, length, prot, flags, fd, offset);
}

int ki_munmap(void* addr, size_t length) {
  return s_kp->munmap(addr, length);
}

int ki_open_resource(const char* file) {
  return s_kp->open_resource(file);
}
