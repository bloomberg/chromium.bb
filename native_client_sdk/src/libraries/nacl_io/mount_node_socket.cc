// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <errno.h>
#include <string.h>

#include "nacl_io/mount.h"
#include "nacl_io/mount_node_socket.h"
#include "nacl_io/pepper_interface.h"

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_net_address.h"

namespace nacl_io {

MountNodeSocket::MountNodeSocket(Mount* mount)
    : MountNode(mount),
      socket_resource_(0),
      local_addr_(0),
      remote_addr_(0) {
  stat_.st_mode |= S_IFSOCK;
}

void MountNodeSocket::Destroy() {
  if (socket_resource_)
    mount_->ppapi()->ReleaseResource(socket_resource_);
  if (local_addr_)
    mount_->ppapi()->ReleaseResource(local_addr_);
  if (remote_addr_)
    mount_->ppapi()->ReleaseResource(remote_addr_);
}

// Default to always signaled, until socket select support is added.
uint32_t MountNodeSocket::GetEventStatus() {
  return POLLIN | POLLOUT;
}

// Assume that |addr| and |out_addr| are non-NULL.
Error MountNodeSocket::MMap(void* addr,
                            size_t length,
                            int prot,
                            int flags,
                            size_t offset,
                            void** out_addr) {
  return EACCES;
}

// Normal read/write operations on a file
Error MountNodeSocket::Read(size_t offs,
                            void* buf,
                            size_t count,
                            int* out_bytes) {
  return Recv(buf, count, 0, out_bytes);
}

Error MountNodeSocket::Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  if (0 == remote_addr_)
    return EDESTADDRREQ;

  return Send(buf, count, 0, out_bytes);
}

NetAddressInterface* MountNodeSocket::NetAddress() {
  return mount_->ppapi()->GetNetAddressInterface();
}

PP_Resource MountNodeSocket::SockAddrToResource(const struct sockaddr* addr,
                                                socklen_t len) {
  if (AF_INET == addr->sa_family) {
    PP_NetAddress_IPv4 addr4;
    const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(addr);

    if (len != sizeof(sockaddr_in))
      return 0;

    memset(&addr4, 0, sizeof(addr4));

    addr4.port = sin->sin_port;
    memcpy(addr4.addr, &sin->sin_addr, sizeof(addr4.addr));
    return mount_->ppapi()->GetNetAddressInterface()->CreateFromIPv4Address(
        mount_->ppapi()->GetInstance(), &addr4);
  }

  if (AF_INET6 == addr->sa_family) {
    PP_NetAddress_IPv6 addr6;
    const sockaddr_in6* sin = reinterpret_cast<const sockaddr_in6*>(addr);

    if (len != sizeof(sockaddr_in6))
      return 0;

    memset(&addr6, 0, sizeof(addr6));

    addr6.port = sin->sin6_port;
    memcpy(addr6.addr, &sin->sin6_addr, sizeof(addr6.addr));
    return mount_->ppapi()->GetNetAddressInterface()->CreateFromIPv6Address(
        mount_->ppapi()->GetInstance(), &addr6);
  }
  return 0;
}


socklen_t MountNodeSocket::ResourceToSockAddr(PP_Resource addr,
                                              socklen_t len,
                                              struct sockaddr* out_addr) {
  if (0 == addr)
    return 0;

  PP_NetAddress_IPv4 ipv4;
  PP_NetAddress_IPv6 ipv6;

  if (PP_TRUE == NetAddress()->DescribeAsIPv4Address(addr, &ipv4)) {
    sockaddr_in addr4;
    addr4.sin_family = AF_INET;
    addr4.sin_port = ipv4.port;
    memcpy(&addr4.sin_addr, ipv4.addr, sizeof(ipv4.addr));
    memcpy(out_addr, &addr4, len);

    // Returns required size not copied size like getpeername/getsockname.
    return sizeof(sockaddr_in);
  }

  if (PP_TRUE == NetAddress()->DescribeAsIPv6Address(addr, &ipv6)) {
    sockaddr_in6 addr6;
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = ipv6.port;
    memcpy(&addr6.sin6_addr, ipv6.addr, sizeof(ipv6.addr));
    memcpy(out_addr, &addr6, len);

    // Returns required size not copied size like getpeername/getsockname.
    return sizeof(sockaddr_in6);
  }

  return 0;
}

bool MountNodeSocket::IsEquivalentAddress(PP_Resource addr1,
                                          PP_Resource addr2) {
  if (addr1 == addr2)
    return true;

  char data1[sizeof(sockaddr_in6)];
  char data2[sizeof(sockaddr_in6)];

  sockaddr* saddr1 = reinterpret_cast<sockaddr*>(data1);
  sockaddr* saddr2 = reinterpret_cast<sockaddr*>(data2);

  socklen_t len1 = ResourceToSockAddr(addr1, sizeof(data1), saddr1);
  socklen_t len2 = ResourceToSockAddr(addr2, sizeof(data2), saddr2);

  if (len1 != len2)
    return false;

  return memcmp(saddr1, saddr2, len1) == 0;
}


Error MountNodeSocket::Accept(const struct sockaddr* addr, socklen_t len) {
  return ENOSYS;
}

Error MountNodeSocket::Connect(const struct sockaddr* addr, socklen_t len) {
  if (len < 1)
    return EINVAL;

  if (NULL == addr)
    return EFAULT;

  return EOPNOTSUPP;
}

Error MountNodeSocket::Listen(int backlog) {
  return EOPNOTSUPP;
}

Error MountNodeSocket::GetSockOpt(int lvl,
                                  int optname,
                                  void* optval,
                                  socklen_t* len) {
  return EINVAL;
}

Error MountNodeSocket::SetSockOpt(int lvl,
                                  int optname,
                                  const void* optval,
                                  socklen_t len) {
  return EINVAL;
}

Error MountNodeSocket::Bind(const struct sockaddr* addr, socklen_t len) {
  return EINVAL;
}

Error MountNodeSocket::Recv(void* buf, size_t len, int flags, int* out_len) {
  return EINVAL;
}

Error MountNodeSocket::RecvFrom(void* buf,
                                size_t len,
                                int flags,
                                struct sockaddr* src_addr,
                                socklen_t* addrlen,
                                int* out_len) {
  return EOPNOTSUPP;
}

Error MountNodeSocket::Send(const void* buf,
                            size_t len,
                            int flags,
                            int* out_len) {
  return EOPNOTSUPP;
}

Error MountNodeSocket::SendTo(const void* buf,
                              size_t len,
                              int flags,
                              const struct sockaddr* dest_addr,
                              socklen_t addrlen,
                              int* out_len) {
  return EOPNOTSUPP;
}

Error MountNodeSocket::Shutdown(int how) {
  return EOPNOTSUPP;
}


Error MountNodeSocket::GetPeerName(struct sockaddr* addr, socklen_t* len) {
  if (NULL == addr || NULL == len)
    return EFAULT;

  AUTO_LOCK(node_lock_);
  if (remote_addr_ != 0) {
    *len = ResourceToSockAddr(remote_addr_, *len, addr);
    return 0;
  }

  return ENOTCONN;
}

Error MountNodeSocket::GetSockName(struct sockaddr* addr, socklen_t* len) {
  if (NULL == addr || NULL == len)
    return EFAULT;

  AUTO_LOCK(node_lock_);
  if (local_addr_ != 0) {
    *len = ResourceToSockAddr(local_addr_, *len, addr);
    return 0;
  }

  return ENOTCONN;
}


}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API