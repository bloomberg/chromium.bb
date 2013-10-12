// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_UDP_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_UDP_H_

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <ppapi/c/pp_resource.h>
#include <ppapi/c/ppb_udp_socket.h>

#include "nacl_io/event_emitter_udp.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/mount_node_socket.h"

namespace nacl_io {

class MountNodeUDP : public MountNodeSocket {
 public:
  explicit MountNodeUDP(Mount* mount);

 protected:
  virtual Error Init(int open_flags);
  virtual void Destroy();

 public:
  virtual EventEmitterUDP* GetEventEmitter();

  virtual void QueueInput();
  virtual void QueueOutput();

  virtual Error Bind(const struct sockaddr* addr, socklen_t len);
  virtual Error Connect(const HandleAttr& attr,
                        const struct sockaddr* addr,
                        socklen_t len);

 protected:
  virtual Error Recv_Locked(void* buf,
                            size_t len,
                            PP_Resource* addr,
                            int* out_len);

  virtual Error Send_Locked(const void* buf,
                            size_t len,
                            PP_Resource addr,
                            int* out_len);

  ScopedEventEmitterUDP emitter_;
};


}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_UDP_H_
