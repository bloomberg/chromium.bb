// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_FUSE_H_
#define LIBRARIES_NACL_IO_MOUNT_FUSE_H_

#include <map>

#include "nacl_io/fuse.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/typed_mount_factory.h"

namespace nacl_io {

class MountFuse : public Mount {
 protected:
  MountFuse();

  virtual Error Init(const MountInitArgs& args);
  virtual void Destroy();

 public:
  virtual Error Access(const Path& path, int a_mode);
  virtual Error Open(const Path& path, int mode, ScopedMountNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int perm);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

private:
  struct fuse_operations* fuse_ops_;
  void* fuse_user_data_;

  friend class MountNodeFuse;
  friend class FuseMountFactory;
  DISALLOW_COPY_AND_ASSIGN(MountFuse);
};

class MountNodeFuse : public MountNode {
 protected:
  MountNodeFuse(Mount* mount,
                struct fuse_operations* fuse_ops,
                struct fuse_file_info& info,
                const std::string& path);

 public:
  virtual bool CanOpen(int open_flags);
  virtual Error GetStat(struct stat* stat);
  virtual Error VIoctl(int request, va_list args);
  virtual Error Tcflush(int queue_selector);
  virtual Error Tcgetattr(struct termios* termios_p);
  virtual Error Tcsetattr(int optional_actions,
                          const struct termios *termios_p);
  virtual Error GetSize(size_t* out_size);

 protected:
  struct fuse_operations* fuse_ops_;
  struct fuse_file_info info_;
  std::string path_;
};

class MountNodeFuseFile : public MountNodeFuse {
 public:
  MountNodeFuseFile(Mount* mount,
                    struct fuse_operations* fuse_ops,
                    struct fuse_file_info& info,
                    const std::string& path);

 protected:
  virtual void Destroy();

 public:
  virtual Error FSync();
  virtual Error FTruncate(off_t length);
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
  friend class MountFuse;
  DISALLOW_COPY_AND_ASSIGN(MountNodeFuseFile);
};

class MountNodeFuseDir : public MountNodeFuse {
 public:
  MountNodeFuseDir(Mount* mount,
                   struct fuse_operations* fuse_ops,
                   struct fuse_file_info& info,
                   const std::string& path);

 protected:
  virtual void Destroy();

 public:
  virtual Error FSync();
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);

 private:
  static int FillDirCallback(void* buf,
                             const char* name,
                             const struct stat* stbuf,
                             off_t off);

 private:
  friend class MountFuse;
  DISALLOW_COPY_AND_ASSIGN(MountNodeFuseDir);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_FUSE_H_
