// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <assert.h>
#include <string.h>
#include <algorithm>

#include "nacl_io/socket/unix_event_emitter.h"
#include "nacl_io/socket/unix_node.h"

namespace nacl_io {

UnixNode::UnixNode(Filesystem* filesystem, int type)
    : SocketNode(SOCK_STREAM, filesystem),
      emitter_(UnixEventEmitter::MakeUnixEventEmitter(65536, type)) {
  emitter_->AttachStream(this);
}

UnixNode::UnixNode(Filesystem* filesystem, const UnixNode& peer)
    : SocketNode(SOCK_STREAM, filesystem),
      emitter_(peer.emitter_->GetPeerEmitter()) {
  emitter_->AttachStream(this);
}

EventEmitter* UnixNode::GetEventEmitter() {
  return emitter_.get();
}

Error UnixNode::Recv_Locked(void* buffer,
                            size_t len,
                            PP_Resource* out_addr,
                            int* out_len) {
  assert(emitter_.get());
  *out_len = emitter_->ReadIn_Locked((char*)buffer, len);
  *out_addr = 0;
  return 0;
}

Error UnixNode::Send_Locked(const void* buffer,
                            size_t len,
                            PP_Resource out_addr,
                            int* out_len) {
  assert(emitter_.get());
  *out_len = emitter_->WriteOut_Locked((char*)buffer, len);
  return 0;
}

Error UnixNode::RecvFrom(const HandleAttr& attr,
                         void* buf,
                         size_t len,
                         int flags,
                         struct sockaddr* src_addr,
                         socklen_t* addrlen,
                         int* out_len) {
  PP_Resource addr = 0;
  Error err = RecvHelper(attr, buf, len, flags, &addr, out_len);
  if (0 == err) {
    if (src_addr) {
      unsigned short family = AF_UNIX;
      memcpy(src_addr, &family,
             std::min(*addrlen, static_cast<socklen_t>(sizeof(family))));
      *addrlen = sizeof(family);
    }
  }

  return err;
}

Error UnixNode::Send(const HandleAttr& attr,
                     const void* buf,
                     size_t len,
                     int flags,
                     int* out_len) {
  PP_Resource addr = 0;
  return SendHelper(attr, buf, len, flags, addr, out_len);
}

Error UnixNode::SendTo(const HandleAttr& attr,
                       const void* buf,
                       size_t len,
                       int flags,
                       const struct sockaddr* dest_addr,
                       socklen_t addrlen,
                       int* out_len) {
  PP_Resource addr = 0;
  return SendHelper(attr, buf, len, flags, addr, out_len);
}

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
