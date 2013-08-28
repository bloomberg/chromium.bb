// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/pepper/ssl_context_helper.h"
#include "content/common/content_export.h"
#include "net/base/address_list.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/host/resource_message_filter.h"

struct PP_NetAddress_Private;

namespace net {
class DrainableIOBuffer;
class IOBuffer;
class SingleRequestHostResolver;
class StreamSocket;
}

namespace ppapi {
class SocketOptionData;

namespace host {
struct ReplyMessageContext;
}
}

namespace content {

class BrowserPpapiHostImpl;
class ResourceContext;

class CONTENT_EXPORT PepperTCPSocketMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperTCPSocketMessageFilter(
      BrowserPpapiHostImpl* host,
      PP_Instance instance,
      bool private_api);

  // Used for creating already connected sockets.  Takes ownership of
  // |socket|.
  PepperTCPSocketMessageFilter(
      BrowserPpapiHostImpl* host,
      PP_Instance instance,
      bool private_api,
      net::StreamSocket* socket);

  static size_t GetNumInstances();

 protected:
  virtual ~PepperTCPSocketMessageFilter();

 private:
  enum State {
    // Before a connection is successfully established (including a previous
    // connect request failed).
    STATE_BEFORE_CONNECT,
    // There is a connect request that is pending.
    STATE_CONNECT_IN_PROGRESS,
    // A connection has been successfully established.
    STATE_CONNECTED,
    // There is an SSL handshake request that is pending.
    STATE_SSL_HANDSHAKE_IN_PROGRESS,
    // An SSL connection has been successfully established.
    STATE_SSL_CONNECTED,
    // An SSL handshake has failed.
    STATE_SSL_HANDSHAKE_FAILED,
    // Socket is closed.
    STATE_CLOSED
  };

  // ppapi::host::ResourceMessageFilter overrides.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  int32_t OnMsgConnect(const ppapi::host::HostMessageContext* context,
                       const std::string& host,
                       uint16_t port);
  int32_t OnMsgConnectWithNetAddress(
      const ppapi::host::HostMessageContext* context,
      const PP_NetAddress_Private& net_addr);
  int32_t OnMsgSSLHandshake(
      const ppapi::host::HostMessageContext* context,
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs);
  int32_t OnMsgRead(const ppapi::host::HostMessageContext* context,
                    int32_t bytes_to_read);
  int32_t OnMsgWrite(const ppapi::host::HostMessageContext* context,
                     const std::string& data);
  int32_t OnMsgDisconnect(const ppapi::host::HostMessageContext* context);
  int32_t OnMsgSetOption(const ppapi::host::HostMessageContext* context,
                         PP_TCPSocket_Option name,
                         const ppapi::SocketOptionData& value);

  void DoConnect(const ppapi::host::ReplyMessageContext& context,
                 const std::string& host,
                 uint16_t port,
                 ResourceContext* resource_context);
  void DoConnectWithNetAddress(
      const ppapi::host::ReplyMessageContext& context,
      const PP_NetAddress_Private& net_addr);
  void DoWrite(const ppapi::host::ReplyMessageContext& context);

  void OnResolveCompleted(const ppapi::host::ReplyMessageContext& context,
                          int net_result);
  void StartConnect(const ppapi::host::ReplyMessageContext& context);

  void OnConnectCompleted(const ppapi::host::ReplyMessageContext& context,
                          int net_result);
  void OnSSLHandshakeCompleted(const ppapi::host::ReplyMessageContext& context,
                               int net_result);
  void OnReadCompleted(const ppapi::host::ReplyMessageContext& context,
                       int net_result);
  void OnWriteCompleted(const ppapi::host::ReplyMessageContext& context,
                        int net_result);

  void SendConnectReply(const ppapi::host::ReplyMessageContext& context,
                        int32_t pp_result,
                        const PP_NetAddress_Private& local_addr,
                        const PP_NetAddress_Private& remote_addr);
  void SendConnectError(const ppapi::host::ReplyMessageContext& context,
                        int32_t pp_error);
  void SendSSLHandshakeReply(const ppapi::host::ReplyMessageContext& context,
                             int32_t pp_result);
  void SendReadReply(const ppapi::host::ReplyMessageContext& context,
                     int32_t pp_result,
                     const std::string& data);
  void SendReadError(const ppapi::host::ReplyMessageContext& context,
                     int32_t pp_error);
  void SendWriteReply(const ppapi::host::ReplyMessageContext& context,
                      int32_t pp_result);

  bool IsConnected() const;
  bool IsSsl() const;
  void SetState(State state);

  bool external_plugin_;
  bool private_api_;

  int render_process_id_;
  int render_view_id_;

  State state_;
  bool end_of_file_reached_;

  scoped_ptr<net::SingleRequestHostResolver> resolver_;
  net::AddressList address_list_;

  scoped_ptr<net::StreamSocket> socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;

  // StreamSocket::Write() may not always write the full buffer, but we would
  // rather have our DoWrite() do so whenever possible. To do this, we may have
  // to call the former multiple times for each of the latter. This entails
  // using a DrainableIOBuffer, which requires an underlying base IOBuffer.
  scoped_refptr<net::IOBuffer> write_buffer_base_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;
  scoped_refptr<SSLContextHelper> ssl_context_helper_;

  DISALLOW_COPY_AND_ASSIGN(PepperTCPSocketMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_MESSAGE_FILTER_H_
