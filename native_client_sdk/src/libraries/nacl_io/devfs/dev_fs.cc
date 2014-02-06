// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(WIN32)
#define _CRT_RAND_S
#endif

#include "nacl_io/devfs/dev_fs.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include "nacl_io/devfs/jspipe_node.h"
#include "nacl_io/devfs/tty_node.h"
#include "nacl_io/dir_node.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/node.h"
#include "nacl_io/osunistd.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/auto_lock.h"

#if defined(__native_client__)
#include <irt.h>
#elif defined(WIN32)
#include <stdlib.h>
#endif

namespace nacl_io {

namespace {

class RealNode : public Node {
 public:
  RealNode(Filesystem* filesystem, int fd);

  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
  virtual Error GetStat(struct stat* stat);

 protected:
  int fd_;
};

class NullNode : public CharNode {
 public:
  explicit NullNode(Filesystem* filesystem) : CharNode(filesystem) {}

  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
};

class ConsoleNode : public CharNode {
 public:
  ConsoleNode(Filesystem* filesystem, PP_LogLevel level);

  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
  PP_LogLevel level_;
};

class ZeroNode : public Node {
 public:
  explicit ZeroNode(Filesystem* filesystem);

  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
};

class UrandomNode : public Node {
 public:
  explicit UrandomNode(Filesystem* filesystem);

  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
#if defined(__native_client__)
  nacl_irt_random random_interface_;
  bool interface_ok_;
#endif
};

RealNode::RealNode(Filesystem* filesystem, int fd) : Node(filesystem), fd_(fd) {
  SetType(S_IFCHR);
}

Error RealNode::Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes) {
  *out_bytes = 0;

  size_t readcnt;
  int err = _real_read(fd_, buf, count, &readcnt);
  if (err)
    return err;

  *out_bytes = static_cast<int>(readcnt);
  return 0;
}

Error RealNode::Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  *out_bytes = 0;

  size_t writecnt;
  int err = _real_write(fd_, buf, count, &writecnt);
  if (err)
    return err;

  *out_bytes = static_cast<int>(writecnt);
  return 0;
}

Error RealNode::GetStat(struct stat* stat) { return _real_fstat(fd_, stat); }

Error NullNode::Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes) {
  *out_bytes = 0;
  return 0;
}

Error NullNode::Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  *out_bytes = count;
  return 0;
}

ConsoleNode::ConsoleNode(Filesystem* filesystem, PP_LogLevel level)
    : CharNode(filesystem), level_(level) {}

Error ConsoleNode::Write(const HandleAttr& attr,
                         const void* buf,
                         size_t count,
                         int* out_bytes) {
  *out_bytes = 0;

  ConsoleInterface* con_intr = filesystem_->ppapi()->GetConsoleInterface();
  VarInterface* var_intr = filesystem_->ppapi()->GetVarInterface();

  if (!(var_intr && con_intr))
    return ENOSYS;

  const char* var_data = static_cast<const char*>(buf);
  uint32_t len = static_cast<uint32_t>(count);
  struct PP_Var val = var_intr->VarFromUtf8(var_data, len);
  con_intr->Log(filesystem_->ppapi()->GetInstance(), level_, val);

  *out_bytes = count;
  return 0;
}

ZeroNode::ZeroNode(Filesystem* filesystem) : Node(filesystem) {
  SetType(S_IFCHR);
}

Error ZeroNode::Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes) {
  memset(buf, 0, count);
  *out_bytes = count;
  return 0;
}

Error ZeroNode::Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  *out_bytes = count;
  return 0;
}

UrandomNode::UrandomNode(Filesystem* filesystem) : Node(filesystem) {
  SetType(S_IFCHR);
#if defined(__native_client__)
  size_t result = nacl_interface_query(
      NACL_IRT_RANDOM_v0_1, &random_interface_, sizeof(random_interface_));
  interface_ok_ = result != 0;
#endif
}

Error UrandomNode::Read(const HandleAttr& attr,
                        void* buf,
                        size_t count,
                        int* out_bytes) {
  *out_bytes = 0;

#if defined(__native_client__)
  if (!interface_ok_)
    return EBADF;

  size_t nread;
  int error = (*random_interface_.get_random_bytes)(buf, count, &nread);
  if (error)
    return error;
#elif defined(WIN32)
  char* out = static_cast<char*>(buf);
  size_t bytes_left = count;
  while (bytes_left) {
    unsigned int random_int;
    errno_t err = rand_s(&random_int);
    if (err) {
      *out_bytes = count - bytes_left;
      return err;
    }

    int bytes_to_copy = std::min(bytes_left, sizeof(random_int));
    memcpy(out, &random_int, bytes_to_copy);
    out += bytes_to_copy;
    bytes_left -= bytes_to_copy;
  }
#endif

  *out_bytes = count;
  return 0;
}

Error UrandomNode::Write(const HandleAttr& attr,
                         const void* buf,
                         size_t count,
                         int* out_bytes) {
  *out_bytes = count;
  return 0;
}

}  // namespace

Error DevFs::Access(const Path& path, int a_mode) {
  ScopedNode node;
  int error = root_->FindChild(path.Join(), &node);
  if (error)
    return error;

  // Don't allow execute access.
  if (a_mode & X_OK)
    return EACCES;

  return 0;
}

Error DevFs::Open(const Path& path, int open_flags, ScopedNode* out_node) {
  out_node->reset(NULL);
  int error = root_->FindChild(path.Join(), out_node);
  // Only return EACCES when trying to create a node that does not exist.
  if ((error == ENOENT) && (open_flags & O_CREAT))
    return EACCES;

  return error;
}

Error DevFs::Unlink(const Path& path) { return EPERM; }

Error DevFs::Mkdir(const Path& path, int permissions) { return EPERM; }

Error DevFs::Rmdir(const Path& path) { return EPERM; }

Error DevFs::Remove(const Path& path) { return EPERM; }

Error DevFs::Rename(const Path& path, const Path& newpath) { return EPERM; }

DevFs::DevFs() {}

#define INITIALIZE_DEV_NODE(path, klass)   \
  new_node = ScopedNode(new klass(this));  \
  error = root_->AddChild(path, new_node); \
  if (error)                               \
    return error;

#define INITIALIZE_DEV_NODE_1(path, klass, arg) \
  new_node = ScopedNode(new klass(this, arg));  \
  error = root_->AddChild(path, new_node);      \
  if (error)                                    \
    return error;

Error DevFs::Init(const FsInitArgs& args) {
  Error error = Filesystem::Init(args);
  if (error)
    return error;

  root_.reset(new DirNode(this));

  ScopedNode new_node;
  INITIALIZE_DEV_NODE("/null", NullNode);
  INITIALIZE_DEV_NODE("/zero", ZeroNode);
  INITIALIZE_DEV_NODE("/urandom", UrandomNode);
  INITIALIZE_DEV_NODE_1("/console0", ConsoleNode, PP_LOGLEVEL_TIP);
  INITIALIZE_DEV_NODE_1("/console1", ConsoleNode, PP_LOGLEVEL_LOG);
  INITIALIZE_DEV_NODE_1("/console2", ConsoleNode, PP_LOGLEVEL_WARNING);
  INITIALIZE_DEV_NODE_1("/console3", ConsoleNode, PP_LOGLEVEL_ERROR);
  INITIALIZE_DEV_NODE("/tty", TtyNode);
  INITIALIZE_DEV_NODE_1("/stdin", RealNode, 0);
  INITIALIZE_DEV_NODE_1("/stdout", RealNode, 1);
  INITIALIZE_DEV_NODE_1("/stderr", RealNode, 2);
  INITIALIZE_DEV_NODE("/jspipe1", JSPipeNode);
  new_node->Ioctl(TIOCNACLPIPENAME, "jspipe1");
  INITIALIZE_DEV_NODE("/jspipe2", JSPipeNode);
  new_node->Ioctl(TIOCNACLPIPENAME, "jspipe2");
  INITIALIZE_DEV_NODE("/jspipe3", JSPipeNode);
  new_node->Ioctl(TIOCNACLPIPENAME, "jspipe3");

  return 0;
}

}  // namespace nacl_io
