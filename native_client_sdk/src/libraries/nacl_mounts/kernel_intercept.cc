/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nacl_mounts/kernel_intercept.h"
#include "nacl_mounts/kernel_proxy.h"

static KernelProxy* s_kp;

void ki_init(void* kp) {
  if (kp == NULL) kp = new KernelProxy();
  s_kp = static_cast<KernelProxy*>(kp);
  s_kp->Init();
}

int ki_chdir(const char* path) {
  return s_kp->chdir(path);
}

char* ki_getcwd(char* buf, size_t size) {
  return s_kp->getcwd(buf, size);
}

char* ki_getwd(char* buf) {
  return s_kp->getwd(buf);
}

int ki_dup(int oldfd) {
  return s_kp->dup(oldfd);
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
  return s_kp->isatty(fd);
}

int ki_close(int fd) {
  return s_kp->close(fd);
}
