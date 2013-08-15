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
#include "nacl_io/mount_node_udp.h"
#include "nacl_io/pepper_interface.h"

namespace nacl_io {

MountNodeUDP::MountNodeUDP(Mount* mount) : MountNodeSocket(mount) {}


UDPSocketInterface* MountNodeUDP::UDPSocket() {
  if (mount_->ppapi() == NULL)
    return NULL;

  return mount_->ppapi()->GetUDPSocketInterface();
}

Error MountNodeUDP::Init(int flags) {
  if (UDPSocket() == NULL)
    return EACCES;

  socket_resource_ = UDPSocket()->Create(mount_->ppapi()->GetInstance());
  if (0 == socket_resource_)
    return EACCES;

  return 0;
}

Error MountNodeUDP::Bind(const struct sockaddr* addr, socklen_t len) {
  if (0 == socket_resource_)
    return EBADF;

  /* Only bind once. */
  if (local_addr_ != 0)
    return EINVAL;

  PP_Resource out_addr = SockAddrToResource(addr, len);
  if (0 == out_addr)
    return EINVAL;

  int err = UDPSocket()->Bind(socket_resource_,
                              out_addr,
                              PP_BlockUntilComplete());
  if (err != 0) {
    mount_->ppapi()->ReleaseResource(out_addr);
    return PPErrorToErrno(err);
  }

  local_addr_ = out_addr;
  return 0;
}

Error MountNodeUDP::Connect(const struct sockaddr* addr, socklen_t len) {
  if (0 == socket_resource_)
    return EBADF;

  /* Connect for UDP is the default dest, it's legal to change it. */
  if (remote_addr_ != 0) {
    mount_->ppapi()->ReleaseResource(remote_addr_);
    remote_addr_ = 0;
  }

  remote_addr_ = SockAddrToResource(addr, len);
  if (0 == remote_addr_)
    return EINVAL;

  return 0;
}

Error MountNodeUDP::RecvFromHelper(void* buf,
                                   size_t len,
                                   int flags,
                                   PP_Resource* out_addr,
                                   int* out_len) {
  if (0 == socket_resource_)
    return EBADF;

  int capped_len = static_cast<int32_t>(std::min(len, MAX_SOCK_TRANSFER));
  int err =  UDPSocket()->RecvFrom(socket_resource_,
                                   static_cast<char*>(buf),
                                   capped_len,
                                   out_addr,
                                   PP_BlockUntilComplete());
  if (err < 0)
    return PPErrorToErrno(err);

  *out_len = err;
  return 0;
}

Error MountNodeUDP::Recv(void* buf, size_t len, int flags, int* out_len) {
  while (1) {
    int local_len = 0;
    PP_Resource addr = 0;

    int err = RecvFromHelper(buf, len, flags, &addr, &local_len);
    if (err < 0)
      return PPErrorToErrno(err);

    /* If "connected" then only receive packets from the given remote. */
    bool same = IsEquivalentAddress(addr, remote_addr_);
    mount_->ppapi()->ReleaseResource(addr);

    if (remote_addr_ != 0 && same)
      continue;

    *out_len = local_len;
    return 0;
  }
}

Error MountNodeUDP::RecvFrom(void* buf,
                             size_t len,
                             int flags,
                             struct sockaddr* src_addr,
                             socklen_t* addrlen,
                             int* out_len) {
  PP_Resource addr = 0;
  int err = RecvFromHelper(buf, len, flags, &addr, out_len);
  if (err < 0)
    return PPErrorToErrno(err);

  if (src_addr)
    *addrlen = ResourceToSockAddr(addr, *addrlen, src_addr);

  mount_->ppapi()->ReleaseResource(addr);
  return 0;
}


Error MountNodeUDP::SendToHelper(const void* buf,
                                 size_t len,
                                 int flags,
                                 PP_Resource addr,
                                 int* out_len) {
  if (0 == socket_resource_)
    return EBADF;

  if (0 == addr)
    return ENOTCONN;

  int capped_len = static_cast<int32_t>(std::min(len, MAX_SOCK_TRANSFER));
  int err = UDPSocket()->SendTo(socket_resource_,
                                static_cast<const char*>(buf),
                                capped_len,
                                addr,
                                PP_BlockUntilComplete());
  if (err < 0)
    return PPErrorToErrno(err);

  *out_len = err;
  return 0;
}

Error MountNodeUDP::Send(const void* buf, size_t len, int flags, int* out_len) {
  return SendToHelper(buf, len, flags, remote_addr_, out_len);
}

Error MountNodeUDP::SendTo(const void* buf,
                           size_t len,
                           int flags,
                           const struct sockaddr* dest_addr,
                           socklen_t addrlen,
                           int* out_len) {
  PP_Resource out_addr = SockAddrToResource(dest_addr, addrlen);
  if (0 == out_addr)
    return EINVAL;

  Error err = SendToHelper(buf, len, flags, out_addr, out_len);
  mount_->ppapi()->ReleaseResource(out_addr);
  return err;
}

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API