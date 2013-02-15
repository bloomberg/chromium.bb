/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#if defined(WIN32)
#define _CRT_RAND_S
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/mount_dev.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/mount_node_dir.h"
#include "nacl_io/pepper_interface.h"
#include "utils/auto_lock.h"

#if defined(__native_client__)
#  include <irt.h>
#elif defined(WIN32)
#  include <stdlib.h>
#endif


namespace {

void ReleaseAndNullNode(MountNode** node) {
  if (*node)
    (*node)->Release();
  *node = NULL;
}


class RealNode : public MountNode {
 public:
  RealNode(Mount* mount, int fd);

  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Write(size_t offs, const void* buf, size_t count);
  virtual int GetStat(struct stat* stat);

 protected:
  int fd_;
};

class NullNode : public MountNode {
 public:
  explicit NullNode(Mount* mount);

  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Write(size_t offs, const void* buf, size_t count);
};

class ConsoleNode : public NullNode {
 public:
  ConsoleNode(Mount* mount, PP_LogLevel level);

  virtual int Write(size_t offs, const void* buf, size_t count);

private:
  PP_LogLevel level_;
};


class TtyNode : public NullNode {
 public:
  explicit TtyNode(Mount* mount);

  virtual int Write(size_t offs, const void* buf, size_t count);
};


class ZeroNode : public MountNode {
 public:
  explicit ZeroNode(Mount* mount);

  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Write(size_t offs, const void* buf, size_t count);
};

class UrandomNode : public MountNode {
 public:
  explicit UrandomNode(Mount* mount);

  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Write(size_t offs, const void* buf, size_t count);

 private:
#if defined(__native_client__)
  nacl_irt_random random_interface_;
  bool interface_ok_;
#endif
};

RealNode::RealNode(Mount* mount, int fd)
    : MountNode(mount),
      fd_(fd) {
  stat_.st_mode = S_IFCHR;
}

int RealNode::Read(size_t offs, void* buf, size_t count) {
  size_t readcnt;
  int err = _real_read(fd_, buf, count, &readcnt);
  if (err) {
    errno = err;
    return -1;
  }
  return static_cast<int>(readcnt);
}

int RealNode::Write(size_t offs, const void* buf, size_t count) {
  size_t writecnt;
  int err = _real_write(fd_, buf, count, &writecnt);
  if (err) {
    errno = err;
    return -1;
  }
  return static_cast<int>(writecnt);
}

int RealNode::GetStat(struct stat* stat) {
  int err =  _real_fstat(fd_, stat);
  if (err) {
    errno = err;
    return -1;
  }
  return 0;
}

NullNode::NullNode(Mount* mount)
    : MountNode(mount) {
  stat_.st_mode = S_IFCHR;
}

int NullNode::Read(size_t offs, void* buf, size_t count) {
  return 0;
}

int NullNode::Write(size_t offs, const void* buf, size_t count) {
  return count;
}

ConsoleNode::ConsoleNode(Mount* mount, PP_LogLevel level)
    : NullNode(mount),
    level_(level) {
  stat_.st_mode = S_IFCHR;
}

int ConsoleNode::Write(size_t offs, const void* buf, size_t count) {
  ConsoleInterface* con_intr = mount_->ppapi()->GetConsoleInterface();
  VarInterface* var_intr = mount_->ppapi()->GetVarInterface();

  if (var_intr && con_intr) {
    const char* data = static_cast<const char *>(buf);
    uint32_t len = static_cast<uint32_t>(count);
    struct PP_Var val = var_intr->VarFromUtf8(data, len);
    con_intr->Log(mount_->ppapi()->GetInstance(), level_, val);
    return count;
  }
  return 0;
}


TtyNode::TtyNode(Mount* mount)
    : NullNode(mount) {
}

int TtyNode::Write(size_t offs, const void* buf, size_t count) {
  MessagingInterface* msg_intr = mount_->ppapi()->GetMessagingInterface();
  VarInterface* var_intr = mount_->ppapi()->GetVarInterface();

  if (var_intr && msg_intr) {
    const char* data = static_cast<const char *>(buf);
    uint32_t len = static_cast<uint32_t>(count);
    struct PP_Var val = var_intr->VarFromUtf8(data, len);
    msg_intr->PostMessage(mount_->ppapi()->GetInstance(), val);
    return count;
  }
  return 0;
}


ZeroNode::ZeroNode(Mount* mount)
    : MountNode(mount) {
  stat_.st_mode = S_IFCHR;
}

int ZeroNode::Read(size_t offs, void* buf, size_t count) {
  memset(buf, 0, count);
  return count;
}

int ZeroNode::Write(size_t offs, const void* buf, size_t count) {
  return count;
}

UrandomNode::UrandomNode(Mount* mount)
    : MountNode(mount) {
  stat_.st_mode = S_IFCHR;
#if defined(__native_client__)
  size_t result = nacl_interface_query(NACL_IRT_RANDOM_v0_1, &random_interface_,
                                       sizeof(random_interface_));
  interface_ok_ = result != 0;
#endif
}

int UrandomNode::Read(size_t offs, void* buf, size_t count) {
#if defined(__native_client__)
  if (interface_ok_) {
    size_t nread;
    int result = (*random_interface_.get_random_bytes)(buf, count, &nread);
    if (result != 0) {
      errno = result;
      return 0;
    }

    return count;
  }

  errno = EBADF;
  return 0;
#elif defined(WIN32)
  char* out = static_cast<char*>(buf);
  size_t bytes_left = count;
  while (bytes_left) {
    unsigned int random_int;
    errno_t err = rand_s(&random_int);
    if (err) {
      errno = err;
      return count - bytes_left;
    }

    int bytes_to_copy = std::min(bytes_left, sizeof(random_int));
    memcpy(out, &random_int, bytes_to_copy);
    out += bytes_to_copy;
    bytes_left -= bytes_to_copy;
  }

  return count;
#endif
}

int UrandomNode::Write(size_t offs, const void* buf, size_t count) {
  return count;
}



}  // namespace

MountNode *MountDev::Open(const Path& path, int mode) {
  AutoLock lock(&lock_);

  // Don't allow creating any files.
  if (mode & O_CREAT)
    return NULL;

  MountNode* node = root_->FindChild(path.Join());
  if (node)
    node->Acquire();
  return node;
}

int MountDev::Unlink(const Path& path) {
  errno = EINVAL;
  return -1;
}

int MountDev::Mkdir(const Path& path, int permissions) {
  errno = EINVAL;
  return -1;
}

int MountDev::Rmdir(const Path& path) {
  errno = EINVAL;
  return -1;
}

int MountDev::Remove(const Path& path) {
  errno = EINVAL;
  return -1;
}

MountDev::MountDev()
    : null_node_(NULL),
      zero_node_(NULL),
      random_node_(NULL),
      console0_node_(NULL),
      console1_node_(NULL),
      console2_node_(NULL),
      console3_node_(NULL),
      tty_node_(NULL) {
}

bool MountDev::Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
  if (!Mount::Init(dev, args, ppapi))
    return false;

  root_ = new MountNodeDir(this);
  null_node_ = new NullNode(this);
  root_->AddChild("/null", null_node_);
  zero_node_ = new ZeroNode(this);
  root_->AddChild("/zero", zero_node_);
  random_node_ = new UrandomNode(this);
  root_->AddChild("/urandom", random_node_);

  console0_node_ = new ConsoleNode(this, PP_LOGLEVEL_TIP);
  root_->AddChild("/console0", console0_node_);
  console1_node_ = new ConsoleNode(this, PP_LOGLEVEL_LOG);
  root_->AddChild("/console1", console1_node_);
  console2_node_ = new ConsoleNode(this, PP_LOGLEVEL_WARNING);
  root_->AddChild("/console2", console2_node_);
  console3_node_ = new ConsoleNode(this, PP_LOGLEVEL_ERROR);
  root_->AddChild("/console3", console3_node_);

  tty_node_ = new TtyNode(this);
  root_->AddChild("/tty", tty_node_);

  stdin_node_ = new RealNode(this, 0);
  root_->AddChild("/stdin", stdin_node_);
  stdout_node_ = new RealNode(this, 1);
  root_->AddChild("/stdout", stdout_node_);
  stderr_node_ = new RealNode(this, 2);
  root_->AddChild("/stderr", stderr_node_);

  return true;
}

void MountDev::Destroy() {
  ReleaseAndNullNode(&stdin_node_);
  ReleaseAndNullNode(&stdout_node_);
  ReleaseAndNullNode(&stderr_node_);
  ReleaseAndNullNode(&tty_node_);
  ReleaseAndNullNode(&console3_node_);
  ReleaseAndNullNode(&console2_node_);
  ReleaseAndNullNode(&console1_node_);
  ReleaseAndNullNode(&console0_node_);
  ReleaseAndNullNode(&random_node_);
  ReleaseAndNullNode(&zero_node_);
  ReleaseAndNullNode(&null_node_);
  ReleaseAndNullNode(&root_);
}
