// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_SOCKET_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_SOCKET_H_

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/ppb_net_address.h>

#include "nacl_io/mount.h"
#include "nacl_io/pepper_interface.h"

namespace nacl_io {

/* Only allow single maximum transfers of 64K or less. Socket users
 * should be looping on Send/Recv size. */
static const size_t MAX_SOCK_TRANSFER = 65536;

class MountSocket;

class MountNodeSocket : public MountNode {
 public:
  explicit MountNodeSocket(Mount* mount);

 protected:
  virtual void Destroy();
  virtual Error Init(int flags) = 0;

 public:
  virtual uint32_t GetEventStatus();

  // Normal read/write operations on a file (recv/send).
  virtual Error Read(size_t offs, void* buf, size_t count, int* out_bytes);
  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

  // Unsuported Functions
  virtual Error Accept(const struct sockaddr* addr, socklen_t len);
  virtual Error Listen(int backlog);
  virtual Error GetSockOpt(int lvl, int optname, void* optval, socklen_t* len);
  virtual Error SetSockOpt(int lvl,
                           int optname,
                           const void* optval,
                           socklen_t len);
  virtual Error Shutdown(int how);
  virtual Error MMap(void* addr,
                     size_t length,
                     int prot,
                     int flags,
                     size_t offset,
                     void** out_addr);

  // Normal Functions.
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

  virtual Error GetPeerName(struct sockaddr* addr, socklen_t* len);
  virtual Error GetSockName(struct sockaddr* addr, socklen_t* len);

 protected:
  NetAddressInterface* NetAddress();
  PP_Resource SockAddrToResource(const struct sockaddr* addr, socklen_t len);
  socklen_t ResourceToSockAddr(PP_Resource addr,
                               socklen_t len,
                               struct sockaddr* out_addr);

  bool IsEquivalentAddress(PP_Resource addr1, PP_Resource addr2);

 protected:
  PP_Resource socket_resource_;
  PP_Resource local_addr_;
  PP_Resource remote_addr_;

  friend class KernelProxy;
  friend class MountSocket;
};


}  // namespace nacl_io


#endif  // PROVIDES_SOCKET_API
#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_SOCKET_H_