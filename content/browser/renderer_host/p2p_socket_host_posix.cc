// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p_socket_host_posix.h"

#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "content/browser/renderer_host/p2p_sockets_host.h"
#include "content/common/p2p_messages.h"
#include "net/base/net_util.h"

namespace {

// This method returns address of the first IPv4 enabled network
// interface it finds ignoring the loopback interface. This address is
// used for all sockets.
//
// TODO(sergeyu): This approach works only in the simplest case when
// host has only one network connection. Instead of binding all
// connections to this interface we must provide list of interfaces to
// the renderer, and let the PortAllocater in the renderer process
// choose local address.
bool GetLocalAddress(sockaddr_in* addr) {
  int fd;
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    LOG(ERROR) << "socket() failed: " << fd;
    return false;
  }

  struct ifconf ifc;
  ifc.ifc_len = 64 * sizeof(struct ifreq);
  scoped_array<char> ifc_buffer(new char[ifc.ifc_len]);
  ifc.ifc_buf = ifc_buffer.get();

  int result = ioctl(fd, SIOCGIFCONF, &ifc);
  if (result < 0) {
    LOG(ERROR) << "GetLocalAddress: ioctl returned error:" << result;
    close(fd);
    return false;
  }
  CHECK_LT(ifc.ifc_len, static_cast<int>(64 * sizeof(struct ifreq)));

  struct ifreq* ptr = reinterpret_cast<struct ifreq*>(ifc.ifc_buf);
  struct ifreq* end =
      reinterpret_cast<struct ifreq*>(ifc.ifc_buf + ifc.ifc_len);

  bool found = false;
  while (ptr < end) {
    struct sockaddr_in* inaddr =
        reinterpret_cast<struct sockaddr_in*>(&ptr->ifr_ifru.ifru_addr);
    if (inaddr->sin_family == AF_INET &&
        strncmp(ptr->ifr_name, "lo", 2) != 0) {
      memcpy(addr, inaddr, sizeof(sockaddr_in));
      found = true;
      break;
    }

    ptr++;
  }

  close(fd);

  return found;
}

bool SocketAddressToSockAddr(P2PSocketAddress address, sockaddr_in* addr) {
  // TODO(sergeyu): Add IPv6 support.
  if (address.address.size() != 4) {
    return false;
  }

  addr->sin_family = AF_INET;
  memcpy(&addr->sin_addr, &address.address[0], 4);
  addr->sin_port = htons(address.port);
  return true;
}

bool SockAddrToSocketAddress(sockaddr_in* addr,
                             P2PSocketAddress* address) {
  if (addr->sin_family != AF_INET) {
    LOG(ERROR) << "SockAddrToSocketAddress: only IPv4 addresses are supported";
    // TODO(sergeyu): Add IPv6 support.
    return false;
  }

  address->address.resize(4);
  memcpy(&address->address[0], &addr->sin_addr, 4);
  address->port = ntohs(addr->sin_port);

  return true;
}

}  // namespace

P2PSocketHostPosix::P2PSocketHostPosix(
    P2PSocketsHost* host, int routing_id, int id)
    : P2PSocketHost(host, routing_id, id),
      state_(STATE_UNINITIALIZED), socket_(0), read_watcher_(this) {
}

P2PSocketHostPosix::~P2PSocketHostPosix() {
  if (state_ == STATE_OPEN) {
    DCHECK_NE(socket_, 0);
    close(socket_);
  }
}

bool P2PSocketHostPosix::Init() {
  socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_ < 0) {
    LOG(ERROR) << "Failed to create socket: " << socket_;
    OnError();
    return false;
  }

  int result = net::SetNonBlocking(socket_);
  if (result < 0) {
    LOG(ERROR) << "Failed to set O_NONBLOCK flag: " << result;
    OnError();
    return false;
  }

  sockaddr_in addr;
  if (!GetLocalAddress(&addr)) {
    LOG(ERROR) << "Failed to get local network address.";
    OnError();
    return false;
  }

  result = bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  if (result < 0) {
    LOG(ERROR) << "bind() failed: " << result;
    OnError();
    return false;
  }

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_READ,
          &read_socket_watcher_, &read_watcher_)) {
    LOG(ERROR) << "WatchFileDescriptor failed on read, errno: " << errno;
    OnError();
    return false;
  }

  socklen_t addrlen = sizeof(addr);
  result = getsockname(socket_, reinterpret_cast<sockaddr*>(&addr),
                       &addrlen);
  if (result < 0) {
    LOG(ERROR) << "P2PSocket::Init(): unable to get local addr: "
               << result;
    OnError();
    return false;
  }

  P2PSocketAddress address;
  if (!SockAddrToSocketAddress(&addr, &address)) {
    OnError();
    return false;
  }

  VLOG(1) << "getsockname() returned "
          << net::NetAddressToString(
              reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
          << ":" << address.port;

  state_ = STATE_OPEN;

  host_->Send(new P2PMsg_OnSocketCreated(routing_id_, id_, address));

  return true;
}

void P2PSocketHostPosix::OnError() {
  if (socket_ != 0) {
    close(socket_);
    socket_ = 0;
  }

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_OPEN) {
    host_->Send(new P2PMsg_OnError(routing_id_, id_));
  }

  state_ = STATE_ERROR;
}

void P2PSocketHostPosix::DidCompleteRead() {
  if (state_ != STATE_OPEN) {
    return;
  }

  std::vector<char> data;
  data.resize(4096);
  sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  int result = recvfrom(socket_, &data[0], data.size(), 0,
                          reinterpret_cast<sockaddr*>(&addr), &addr_len);
  if (result > 0) {
    data.resize(result);
    VLOG(2) << "received " << result << " bytes from "
            << net::NetAddressToString(
                reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
            << ":" << ntohs(addr.sin_port);
    P2PSocketAddress address;
    if (!SockAddrToSocketAddress(&addr, &address)) {
      // Address conversion fails only if we receive a non-IPv4
      // packet, which should never happen because the socket is IPv4.
      NOTREACHED();
      return;
    }

    host_->Send(new P2PMsg_OnDataReceived(routing_id_, id_,
                                               address, data));
  } else if (result < 0) {
    LOG(ERROR) << "recvfrom() returned error: " << result;
    OnError();
  }
}

void P2PSocketHostPosix::Send(P2PSocketAddress socket_address,
                              const std::vector<char>& data) {
  sockaddr_in addr;
  SocketAddressToSockAddr(socket_address, &addr);
  int result = sendto(socket_, &data[0], data.size(), 0,
                      reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  if (result < 0) {
    LOG(ERROR) << "Send failed.";
  } else {
    VLOG(2) << "Sent " << result << " bytes to "
            << net::NetAddressToString(
                reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
            << ":" <<  ntohs(addr.sin_port);
  }
}

// static
P2PSocketHost* P2PSocketHost::Create(
    P2PSocketsHost* host, int routing_id, int id, P2PSocketType type) {
  switch (type) {
    case P2P_SOCKET_UDP:
      return new P2PSocketHostPosix(host, routing_id, id);

    case P2P_SOCKET_TCP_SERVER:
      // TODO(sergeyu): Implement TCP sockets support.
      return NULL;

    case P2P_SOCKET_TCP_CLIENT:
      return NULL;
  }

  NOTREACHED();
  return NULL;
}
