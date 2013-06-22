// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_tcp_socket.h"

struct PP_NetAddress_Private;

namespace ppapi {
class PPB_X509Certificate_Fields;
class SocketOptionData;
}

namespace net {
class DrainableIOBuffer;
class IOBuffer;
class SingleRequestHostResolver;
class StreamSocket;
class X509Certificate;
}

namespace content {
class PepperMessageFilter;

// PepperTCPSocket is used by PepperMessageFilter to handle requests from
// the Pepper TCP socket API (PPB_TCPSocket and PPB_TCPSocket_Private).
class PepperTCPSocket {
 public:
  PepperTCPSocket(PepperMessageFilter* manager,
                  int32 routing_id,
                  uint32 plugin_dispatcher_id,
                  uint32 socket_id,
                  bool private_api);

  // Used for creation already connected sockets.  Takes ownership of
  // |socket|.
  PepperTCPSocket(PepperMessageFilter* manager,
                  int32 routing_id,
                  uint32 plugin_dispatcher_id,
                  uint32 socket_id,
                  net::StreamSocket* socket,
                  bool private_api);
  ~PepperTCPSocket();

  int routing_id() { return routing_id_; }
  bool private_api() const { return private_api_; }

  void Connect(const std::string& host, uint16_t port);
  void ConnectWithNetAddress(const PP_NetAddress_Private& net_addr);
  void SSLHandshake(
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs);
  void Read(int32 bytes_to_read);
  void Write(const std::string& data);
  void SetOption(PP_TCPSocket_Option name,
                 const ppapi::SocketOptionData& value);

  void SendConnectACKError(int32_t error);

  // Extracts the certificate field data from a |net::X509Certificate| into
  // |PPB_X509Certificate_Fields|.
  static bool GetCertificateFields(const net::X509Certificate& cert,
                                   ppapi::PPB_X509Certificate_Fields* fields);
  // Extracts the certificate field data from the DER representation of a
  // certificate into |PPB_X509Certificate_Fields|.
  static bool GetCertificateFields(const char* der,
                                   uint32_t length,
                                   ppapi::PPB_X509Certificate_Fields* fields);

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

  void SendReadACKError(int32_t error);
  void SendWriteACKError(int32_t error);
  void SendSSLHandshakeACK(bool succeeded);
  void SendSetOptionACK(int32_t result);

  void OnResolveCompleted(int net_result);
  void OnConnectCompleted(int net_result);
  void OnSSLHandshakeCompleted(int net_result);
  void OnReadCompleted(int net_result);
  void OnWriteCompleted(int net_result);

  bool IsConnected() const;
  bool IsSsl() const;

  // Actually does a write from |write_buffer_|; possibly called many times for
  // each |Write()|.
  void DoWrite();

  PepperMessageFilter* manager_;
  int32 routing_id_;
  uint32 plugin_dispatcher_id_;
  uint32 socket_id_;
  bool private_api_;

  ConnectionState connection_state_;
  bool end_of_file_reached_;

  scoped_ptr<net::SingleRequestHostResolver> resolver_;
  net::AddressList address_list_;

  scoped_ptr<net::StreamSocket> socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;

  // |StreamSocket::Write()| may not always write the full buffer, but we would
  // rather have our |Write()| do so whenever possible. To do this, we may have
  // to call the former multiple times for each of the latter. This entails
  // using a |DrainableIOBuffer|, which requires an underlying base |IOBuffer|.
  scoped_refptr<net::IOBuffer> write_buffer_base_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PepperTCPSocket);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_H_
