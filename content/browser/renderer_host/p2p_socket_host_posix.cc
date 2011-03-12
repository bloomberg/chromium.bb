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

  net::IPEndPoint address;
  if (!address.FromSockAddr(reinterpret_cast<sockaddr*>(&addr), addrlen)) {
    OnError();
    return false;
  }

  VLOG(1) << "getsockname() returned "
          << address.ToString();

  state_ = STATE_OPEN;

  host_->Send(new P2PMsg_OnSocketCreated(routing_id_, id_, address));

  return true;
}

void P2PSocketHostPosix::OnError() {
  if (socket_ > 0)
    close(socket_);
  socket_ = 0;

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_OPEN)
    host_->Send(new P2PMsg_OnError(routing_id_, id_));

  state_ = STATE_ERROR;
}

void P2PSocketHostPosix::DidCompleteRead() {
  if (state_ != STATE_OPEN)
    return;

  std::vector<char> data;
  data.resize(4096);
  sockaddr_storage addr_storage;
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  socklen_t addr_len = sizeof(addr_storage);
  int result = recvfrom(socket_, &data[0], data.size(), 0, addr, &addr_len);
  if (result > 0) {
    data.resize(result);

    net::IPEndPoint address;
    if (!address.FromSockAddr(addr, addr_len) ||
        address.GetFamily() != AF_INET) {
      // We should never receive IPv6 packet on IPv4 packet.
      NOTREACHED();
      return;
    }

    VLOG(2) << "received " << result << " bytes from "
            << address.ToString();

    host_->Send(new P2PMsg_OnDataReceived(routing_id_, id_,
                                               address, data));
  } else if (result < 0) {
    LOG(ERROR) << "recvfrom() returned error: " << result;
    OnError();
  }
}

void P2PSocketHostPosix::Send(const net::IPEndPoint& socket_address,
                              const std::vector<char>& data) {
  sockaddr_storage addr_storage;
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  size_t addr_len = sizeof(addr_storage);
  socket_address.ToSockAddr(addr, &addr_len);
  int result = sendto(socket_, &data[0], data.size(), 0, addr, addr_len);
  if (result < 0) {
    LOG(ERROR) << "Send failed.";
  } else {
    VLOG(2) << "Sent " << result << " bytes to "
            << socket_address.ToString();
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
