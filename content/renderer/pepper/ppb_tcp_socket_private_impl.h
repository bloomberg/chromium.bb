// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_TCP_SOCKET_PRIVATE_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_TCP_SOCKET_PRIVATE_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "ipc/ipc_listener.h"
#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"

namespace content {

class PPB_TCPSocket_Private_Impl : public ppapi::TCPSocketPrivateImpl,
                                   public IPC::Listener {
 public:
  static PP_Resource CreateResource(PP_Instance instance);

  virtual void SendConnect(const std::string& host, uint16_t port) OVERRIDE;
  virtual void SendConnectWithNetAddress(
      const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void SendSSLHandshake(
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs) OVERRIDE;
  virtual void SendRead(int32_t bytes_to_read) OVERRIDE;
  virtual void SendWrite(const std::string& buffer) OVERRIDE;
  virtual void SendDisconnect() OVERRIDE;
  virtual void SendSetOption(PP_TCPSocket_Option name,
                             const ppapi::SocketOptionData& value) OVERRIDE;

 private:
  PPB_TCPSocket_Private_Impl(PP_Instance instance,
                             uint32 socket_id,
                             int routing_id);
  virtual ~PPB_TCPSocket_Private_Impl();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnTCPSocketConnectACK(uint32 plugin_dispatcher_id,
                             uint32 socket_id,
                             int32_t result,
                             const PP_NetAddress_Private& local_addr,
                             const PP_NetAddress_Private& remote_addr);
  void OnTCPSocketSSLHandshakeACK(
      uint32 plugin_dispatcher_id,
      uint32 socket_id,
      bool succeeded,
      const ppapi::PPB_X509Certificate_Fields& certificate_fields);
  void OnTCPSocketReadACK(uint32 plugin_dispatcher_id,
                          uint32 socket_id,
                          int32_t result,
                          const std::string& data);
  void OnTCPSocketWriteACK(uint32 plugin_dispatcher_id,
                           uint32 socket_id,
                           int32_t result);
  void OnTCPSocketSetOptionACK(uint32 plugin_dispatcher_id,
                               uint32 socket_id,
                               int32_t result);

  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(PPB_TCPSocket_Private_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_TCP_SOCKET_PRIVATE_IMPL_H_
