// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_TCP_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_TCP_H_

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <ppapi/c/pp_resource.h>
#include <ppapi/c/ppb_tcp_socket.h>

#include "nacl_io/mount_node.h"
#include "nacl_io/mount_node_socket.h"

namespace nacl_io {

class MountNodeTCP : public MountNodeSocket {
 public:
  explicit MountNodeTCP(Mount* mount);

  virtual Error Init(int flags);

  virtual Error Bind(const struct sockaddr* addr, socklen_t len);
  virtual Error Connect(const struct sockaddr* addr, socklen_t len);

  virtual Error Recv(void* buf, size_t len, int flags, int* out_len);
  virtual Error RecvFrom(void* buf,
                         size_t len,
                         int flags,
                         struct sockaddr* src_addr,
                         socklen_t* addrlen,
                         int* out_len);

  virtual Error Send(const void* buf, size_t len, int flags, int* out_len);
  virtual Error SendTo(const void* buf,
                       size_t len,
                       int flags,
                       const struct sockaddr* dest_addr,
                       socklen_t addrlen,
                       int* out_len);

 protected:
  TCPSocketInterface* TCPSocket();
};


}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_TCP_H_
