// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(WIN32)
#define _CRT_RAND_S
#endif

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include <deque>

#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/mount_dev.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/mount_node_dir.h"
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

class RealNode : public MountNode {
 public:
  RealNode(Mount* mount, int fd);

  virtual Error Read(size_t offs, void* buf, size_t count, int* out_bytes);
  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
  virtual Error GetStat(struct stat* stat);

 protected:
  int fd_;
};

class NullNode : public MountNode {
 public:
  explicit NullNode(Mount* mount);

  virtual Error Read(size_t offs, void* buf, size_t count, int* out_bytes);
  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
};

class ConsoleNode : public NullNode {
 public:
  ConsoleNode(Mount* mount, PP_LogLevel level);

  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
  PP_LogLevel level_;
};

class TtyNode : public NullNode {
 public:
  explicit TtyNode(Mount* mount);
  ~TtyNode();

  virtual Error Ioctl(int request,
                      char* arg);

  virtual Error Read(size_t offs,
                     void* buf,
                     size_t count,
                     int* out_bytes);

  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
  std::deque<char> input_buffer_;
  pthread_cond_t is_readable_;
  std::string prefix_;
};

class ZeroNode : public MountNode {
 public:
  explicit ZeroNode(Mount* mount);

  virtual Error Read(size_t offs, void* buf, size_t count, int* out_bytes);
  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
};

class UrandomNode : public MountNode {
 public:
  explicit UrandomNode(Mount* mount);

  virtual Error Read(size_t offs, void* buf, size_t count, int* out_bytes);
  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
#if defined(__native_client__)
  nacl_irt_random random_interface_;
  bool interface_ok_;
#endif
};

RealNode::RealNode(Mount* mount, int fd) : MountNode(mount), fd_(fd) {
  stat_.st_mode = S_IFCHR;
}

Error RealNode::Read(size_t offs, void* buf, size_t count, int* out_bytes) {
  *out_bytes = 0;

  size_t readcnt;
  int err = _real_read(fd_, buf, count, &readcnt);
  if (err)
    return err;

  *out_bytes = static_cast<int>(readcnt);
  return 0;
}

Error RealNode::Write(size_t offs,
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

NullNode::NullNode(Mount* mount) : MountNode(mount) { stat_.st_mode = S_IFCHR; }

Error NullNode::Read(size_t offs, void* buf, size_t count, int* out_bytes) {
  *out_bytes = 0;
  return 0;
}

Error NullNode::Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  *out_bytes = count;
  return 0;
}

ConsoleNode::ConsoleNode(Mount* mount, PP_LogLevel level)
    : NullNode(mount), level_(level) {
  stat_.st_mode = S_IFCHR;
}

Error ConsoleNode::Write(size_t offs,
                         const void* buf,
                         size_t count,
                         int* out_bytes) {
  *out_bytes = 0;

  ConsoleInterface* con_intr = mount_->ppapi()->GetConsoleInterface();
  VarInterface* var_intr = mount_->ppapi()->GetVarInterface();

  if (!(var_intr && con_intr))
    return ENOSYS;

  const char* data = static_cast<const char*>(buf);
  uint32_t len = static_cast<uint32_t>(count);
  struct PP_Var val = var_intr->VarFromUtf8(data, len);
  con_intr->Log(mount_->ppapi()->GetInstance(), level_, val);

  *out_bytes = count;
  return 0;
}

TtyNode::TtyNode(Mount* mount) : NullNode(mount) {
  pthread_cond_init(&is_readable_, NULL);
}

TtyNode::~TtyNode() {
  pthread_cond_destroy(&is_readable_);
}

Error TtyNode::Write(size_t offs,
                     const void* buf,
                     size_t count,
                     int* out_bytes) {
  *out_bytes = 0;

  MessagingInterface* msg_intr = mount_->ppapi()->GetMessagingInterface();
  VarInterface* var_intr = mount_->ppapi()->GetVarInterface();

  if (!(var_intr && msg_intr))
    return ENOSYS;

  // We append the prefix_ to the data in buf, then package it up
  // and post it as a message.
  const char* data = static_cast<const char*>(buf);
  std::string message;
  {
    AUTO_LOCK(node_lock_);
    message = prefix_;
  }
  message.append(data, count);
  uint32_t len = static_cast<uint32_t>(message.size());
  struct PP_Var val = var_intr->VarFromUtf8(message.data(), len);
  msg_intr->PostMessage(mount_->ppapi()->GetInstance(), val);
  *out_bytes = count;
  return 0;
}

Error TtyNode::Read(size_t offs, void* buf, size_t count, int* out_bytes) {
  AUTO_LOCK(node_lock_);
  while (input_buffer_.size() <= 0) {
    pthread_cond_wait(&is_readable_, node_lock_.mutex());
  }

  // Copies data from the input buffer into buf.
  size_t bytes_to_copy = std::min(count, input_buffer_.size());
  std::copy(input_buffer_.begin(), input_buffer_.begin() + bytes_to_copy,
            static_cast<char*>(buf));
  *out_bytes = bytes_to_copy;
  input_buffer_.erase(input_buffer_.begin(),
                      input_buffer_.begin() + bytes_to_copy);
  return 0;
}

Error TtyNode::Ioctl(int request, char* arg) {
  if (request == TIOCNACLPREFIX) {
    // This ioctl is used to change the prefix for this tty node.
    // The prefix is used to distinguish messages intended for this
    // tty node from all the other messages cluttering up the
    // javascript postMessage() channel.
    AUTO_LOCK(node_lock_);
    prefix_ = arg;
    return 0;
  } else if (request == TIOCNACLINPUT) {
    // This ioctl is used to deliver data from the user to this tty node's
    // input buffer. We check if the prefix in the input data matches the
    // prefix for this node, and only deliver the data if so.
    struct tioc_nacl_input_string* message =
      reinterpret_cast<struct tioc_nacl_input_string*>(arg);
    AUTO_LOCK(node_lock_);
    if (message->length >= prefix_.size() &&
        strncmp(message->buffer, prefix_.data(), prefix_.size()) == 0) {
      input_buffer_.insert(input_buffer_.end(),
                           message->buffer + prefix_.size(),
                           message->buffer + message->length);
      pthread_cond_broadcast(&is_readable_);
      return 0;
    }
    return ENOTTY;
  } else {
    return EINVAL;
  }
}

ZeroNode::ZeroNode(Mount* mount) : MountNode(mount) { stat_.st_mode = S_IFCHR; }

Error ZeroNode::Read(size_t offs, void* buf, size_t count, int* out_bytes) {
  memset(buf, 0, count);
  *out_bytes = count;
  return 0;
}

Error ZeroNode::Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  *out_bytes = count;
  return 0;
}

UrandomNode::UrandomNode(Mount* mount) : MountNode(mount) {
  stat_.st_mode = S_IFCHR;
#if defined(__native_client__)
  size_t result = nacl_interface_query(
      NACL_IRT_RANDOM_v0_1, &random_interface_, sizeof(random_interface_));
  interface_ok_ = result != 0;
#endif
}

Error UrandomNode::Read(size_t offs, void* buf, size_t count, int* out_bytes) {
  *out_bytes = 0;

#if defined(__native_client__)
  if (!interface_ok_)
    return EBADF;

  size_t nread;
  int error = (*random_interface_.get_random_bytes)(buf, count, &nread);
  if (error)
    return error;

  *out_bytes = count;
  return 0;
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

  *out_bytes = count;
  return 0;
#endif
}

Error UrandomNode::Write(size_t offs,
                         const void* buf,
                         size_t count,
                         int* out_bytes) {
  *out_bytes = count;
  return 0;
}

}  // namespace

Error MountDev::Access(const Path& path, int a_mode) {
  ScopedMountNode node;
  int error = root_->FindChild(path.Join(), &node);
  if (error)
    return error;

  // Don't allow execute access.
  if (a_mode & X_OK)
    return EACCES;

  return 0;
}

Error MountDev::Open(const Path& path, int mode, ScopedMountNode* out_node) {
  out_node->reset(NULL);

  // Don't allow creating any files.
  if (mode & O_CREAT)
    return EINVAL;

  return root_->FindChild(path.Join(), out_node);
}

Error MountDev::Unlink(const Path& path) { return EINVAL; }

Error MountDev::Mkdir(const Path& path, int permissions) { return EINVAL; }

Error MountDev::Rmdir(const Path& path) { return EINVAL; }

Error MountDev::Remove(const Path& path) { return EINVAL; }

MountDev::MountDev() {}

#define INITIALIZE_DEV_NODE(path, klass)                           \
  error = root_->AddChild(path, ScopedMountNode(new klass(this))); \
  if (error)                                                       \
    return error;

#define INITIALIZE_DEV_NODE_1(path, klass, arg)                         \
  error = root_->AddChild(path, ScopedMountNode(new klass(this, arg))); \
  if (error)                                                            \
    return error;

Error MountDev::Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
  Error error = Mount::Init(dev, args, ppapi);
  if (error)
    return error;

  root_.reset(new MountNodeDir(this));

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

  return 0;
}

}  // namespace nacl_io

