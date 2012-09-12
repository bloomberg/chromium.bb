/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_KERNEL_PROXY_H_
#define LIBRARIES_NACL_MOUNTS_KERNEL_PROXY_H_

#include <pthread.h>
#include <map>
#include <string>
#include <vector>

#include "nacl_mounts/path.h"
#include "nacl_mounts/kernel_object.h"
#include "nacl_mounts/mount.h"
#include "nacl_mounts/ostypes.h"

class KernelHandle;
class Mount;
class MountNode;

// KernelProxy provide one-to-one mapping for libc kernel calls.  Calls to the
// proxy will result in IO access to the provided Mount and MountNode objects.
class KernelProxy : protected KernelObject {
 public:
  typedef Mount* (*MountFactory_t)(int, StringMap_t&);
  typedef std::map<std::string, std::string> StringMap_t;
  typedef std::map<std::string, MountFactory_t> MountFactoryMap_t;

  KernelProxy();
  virtual ~KernelProxy();
  virtual void Init();

  // KernelHandle and FD allocation and manipulation functions.
  virtual int open(const char *path, int oflag);
  virtual int close(int fd);
  virtual int dup(int fd);

  // System calls handled by KernelProxy (not mount-specific)
  virtual int chdir(const char* path);
  virtual char* getcwd(char* buf, size_t size);
  virtual char* getwd(char* buf);
  virtual int mount(const char *source, const char *target, 
      const char *filesystemtype, unsigned long mountflags, const void *data);
  virtual int umount(const char *path);

  // System calls that take a path as an argument:
  // The kernel proxy will look for the Node associated to the path. To
  // find the node, the kernel proxy calls the corresponding mount's GetNode()
  // method. The corresponding  method will be called. If the node
  // cannot be found, errno is set and -1 is returned.
  virtual int chmod(const char *path, mode_t mode);
  virtual int mkdir(const char *path, mode_t mode);
  virtual int rmdir(const char *path);
  virtual int stat(const char *path, struct stat *buf);

  // System calls that take a file descriptor as an argument:
  // The kernel proxy will determine to which mount the file
  // descriptor's corresponding file handle belongs.  The
  // associated mount's function will be called.
  virtual ssize_t read(int fd, void *buf, size_t nbyte);
  virtual ssize_t write(int fd, const void *buf, size_t nbyte);

  virtual int fchmod(int fd, int prot);
  virtual int fstat(int fd, struct stat *buf);
  virtual int getdents(int fd, void *buf, unsigned int count);
  virtual int fsync(int fd);
  virtual int isatty(int fd);

  // lseek() relies on the mount's Stat() to determine whether or not the
  // file handle corresponding to fd is a directory
  virtual off_t lseek(int fd, off_t offset, int whence);

  // remove() uses the mount's GetNode() and Stat() to determine whether or
  // not the path corresponds to a directory or a file.  The mount's Rmdir()
  // or Unlink() is called accordingly.
  virtual int remove(const char* path);
  // unlink() is a simple wrapper around the mount's Unlink function.
  virtual int unlink(const char* path);
  // access() uses the Mount's Stat().
  int access(const char*path, int amode);

protected:
  MountFactoryMap_t factories_;
  int dev_;

  static KernelProxy *s_instance_;

  DISALLOW_COPY_AND_ASSIGN(KernelProxy);
};

#endif  // LIBRARIES_NACL_MOUNTS_KERNEL_PROXY_H_
