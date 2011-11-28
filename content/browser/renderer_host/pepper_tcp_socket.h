// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_TCP_SOCKET_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_TCP_SOCKET_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "ppapi/c/pp_stdint.h"

class PepperMessageFilter;
struct PP_NetAddress_Private;

namespace net {
class IOBuffer;
class SingleRequestHostResolver;
class StreamSocket;
}

// PepperTCPSocket is used by PepperMessageFilter to handle requests from
// the Pepper TCP socket API (PPB_TCPSocket_Private).
class PepperTCPSocket {
 public:
  PepperTCPSocket(PepperMessageFilter* manager,
                  int32 routing_id,
                  uint32 plugin_dispatcher_id,
                  uint32 socket_id);
  ~PepperTCPSocket();

  void Connect(const std::string& host, uint16_t port);
  void ConnectWithNetAddress(const PP_NetAddress_Private& net_addr);
  void SSLHandshake(const std::string& server_name, uint16_t server_port);
  void Read(int32 bytes_to_read);
  void Write(const std::string& data);

 private:
  enum ConnectionState {
    // Before a connection is successfully established (including a previous
    // connect request failed).
    BEFORE_CONNECT,
    // There is a connect request that is pending.
    CONNECT_IN_PROGRESS,
    // A connection has been successfully established.
    CONNECTED,
    // There is an SSL handshake request that is pending.
    SSL_HANDSHAKE_IN_PROGRESS,
    // An SSL connection has been successfully established.
    SSL_CONNECTED,
    // An SSL handshake has failed.
    SSL_HANDSHAKE_FAILED
  };

  void StartConnect(const net::AddressList& addresses);

  void SendConnectACKError();
  void SendReadACKError();
  void SendWriteACKError();
  void SendSSLHandshakeACK(bool succeeded);

  void OnResolveCompleted(int result);
  void OnConnectCompleted(int result);
  void OnSSLHandshakeCompleted(int result);
  void OnReadCompleted(int result);
  void OnWriteCompleted(int result);

  bool IsConnected() const;

  PepperMessageFilter* manager_;
  int32 routing_id_;
  uint32 plugin_dispatcher_id_;
  uint32 socket_id_;

  ConnectionState connection_state_;
  bool end_of_file_reached_;

  net::OldCompletionCallbackImpl<PepperTCPSocket> connect_callback_;
  net::OldCompletionCallbackImpl<PepperTCPSocket> ssl_handshake_callback_;
  net::OldCompletionCallbackImpl<PepperTCPSocket> read_callback_;
  net::OldCompletionCallbackImpl<PepperTCPSocket> write_callback_;

  scoped_ptr<net::SingleRequestHostResolver> resolver_;
  net::AddressList address_list_;

  scoped_ptr<net::StreamSocket> socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::IOBuffer> write_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PepperTCPSocket);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_TCP_SOCKET_H_
