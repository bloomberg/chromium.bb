// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <errno.h>
#include <string.h>
#include <algorithm>

#include "nacl_io/mount.h"
#include "nacl_io/mount_node_socket.h"
#include "nacl_io/mount_node_tcp.h"
#include "nacl_io/pepper_interface.h"

namespace nacl_io {

MountNodeTCP::MountNodeTCP(Mount* mount) : MountNodeSocket(mount) {}


TCPSocketInterface* MountNodeTCP::TCPSocket() {
  if (mount_->ppapi() == NULL)
    return NULL;

  return mount_->ppapi()->GetTCPSocketInterface();
}

Error MountNodeTCP::Init(int flags) {
  if (TCPSocket() == NULL)
    return EACCES;

  socket_resource_ = TCPSocket()->Create(mount_->ppapi()->GetInstance());
  if (0 == socket_resource_)
    return EACCES;

  return 0;
}

Error MountNodeTCP::Bind(const struct sockaddr* addr, socklen_t len) {
  AUTO_LOCK(node_lock_);

  if (0 == socket_resource_)
    return EBADF;

  /* Only bind once. */
  if (local_addr_ != 0)
    return EINVAL;

  /* Lie, we won't know until we connect. */
  return 0;
}

Error MountNodeTCP::Connect(const struct sockaddr* addr, socklen_t len) {
  AUTO_LOCK(node_lock_);

  if (0 == socket_resource_)
    return EBADF;

  if (remote_addr_ != 0)
    return EISCONN;

  remote_addr_ = SockAddrToResource(addr, len);
  if (0 == remote_addr_)
    return EINVAL;

  int err = TCPSocket()->Connect(socket_resource_,
                                 remote_addr_,
                                 PP_BlockUntilComplete());

  // If we fail, release the dest addr resource
  if (err != PP_OK) {
    mount_->ppapi()->ReleaseResource(remote_addr_);
    remote_addr_ = 0;
    return PPErrorToErrno(err);
  }

  local_addr_ = TCPSocket()->GetLocalAddress(socket_resource_);
  mount_->ppapi()->AddRefResource(local_addr_);
  return 0;
}

Error MountNodeTCP::Recv(void* buf, size_t len, int flags, int* out_len) {
  AUTO_LOCK(node_lock_);
  if (0 == socket_resource_)
    return EBADF;

  int capped_len = static_cast<int32_t>(std::min(len, MAX_SOCK_TRANSFER));
  int err = TCPSocket()->Read(socket_resource_,
                              static_cast<char*>(buf),
                              capped_len,
                              PP_BlockUntilComplete());
  if (err < 0)
    return PPErrorToErrno(err);

  *out_len = err;
  return 0;
}

Error MountNodeTCP::RecvFrom(void* buf,
                             size_t len,
                             int flags,
                             struct sockaddr* src_addr,
                             socklen_t* addrlen,
                             int* out_len) {
  Error err = Recv(buf, len, flags, out_len);
  if (err == 0)
    GetPeerName(src_addr, addrlen);
  return err;
}


Error MountNodeTCP::Send(const void* buf, size_t len, int flags, int* out_len) {
  AUTO_LOCK(node_lock_);

  if (0 == socket_resource_)
    return EBADF;

  if (0 == remote_addr_)
    return ENOTCONN;

  int capped_len = static_cast<int32_t>(std::min(len, MAX_SOCK_TRANSFER));
  int err = TCPSocket()->Write(socket_resource_,
                               static_cast<const char*>(buf),
                               capped_len,
                               PP_BlockUntilComplete());
  if (err < 0)
    return PPErrorToErrno(err);

  *out_len = err;
  return 0;
}

Error MountNodeTCP::SendTo(const void* buf,
                           size_t len,
                           int flags,
                           const struct sockaddr* dest_addr,
                           socklen_t addrlen,
                           int* out_len) {
  return Send(buf, len, flags, out_len);
}

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API