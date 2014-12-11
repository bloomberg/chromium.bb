// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_MESSAGE_FILTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/process_type.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/udp/udp_socket.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/host/resource_message_filter.h"

struct PP_NetAddress_Private;

namespace net {
class IOBuffer;
class IOBufferWithSize;
}

namespace ppapi {

class SocketOptionData;

namespace host {
struct ReplyMessageContext;
}
}

namespace content {

class BrowserPpapiHostImpl;
struct SocketPermissionRequest;

class CONTENT_EXPORT PepperUDPSocketMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperUDPSocketMessageFilter(BrowserPpapiHostImpl* host,
                               PP_Instance instance,
                               bool private_api);

  static size_t GetNumInstances();

 protected:
  ~PepperUDPSocketMessageFilter() override;

 private:
  enum SocketOption {
    SOCKET_OPTION_ADDRESS_REUSE = 1 << 0,
    SOCKET_OPTION_BROADCAST = 1 << 1,
    SOCKET_OPTION_RCVBUF_SIZE = 1 << 2,
    SOCKET_OPTION_SNDBUF_SIZE = 1 << 3
  };

  // ppapi::host::ResourceMessageFilter overrides.
  scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  int32_t OnMsgSetOption(const ppapi::host::HostMessageContext* context,
                         PP_UDPSocket_Option name,
                         const ppapi::SocketOptionData& value);
  int32_t OnMsgBind(const ppapi::host::HostMessageContext* context,
                    const PP_NetAddress_Private& addr);
  int32_t OnMsgSendTo(const ppapi::host::HostMessageContext* context,
                      const std::string& data,
                      const PP_NetAddress_Private& addr);
  int32_t OnMsgClose(const ppapi::host::HostMessageContext* context);
  int32_t OnMsgRecvSlotAvailable(
      const ppapi::host::HostMessageContext* context);

  void DoBind(const ppapi::host::ReplyMessageContext& context,
              const PP_NetAddress_Private& addr);
  void DoRecvFrom();
  void DoSendTo(const ppapi::host::ReplyMessageContext& context,
                const std::string& data,
                const PP_NetAddress_Private& addr);
  void Close();

  void OnRecvFromCompleted(int net_result);
  void OnSendToCompleted(const ppapi::host::ReplyMessageContext& context,
                         int net_result);

  void SendBindReply(const ppapi::host::ReplyMessageContext& context,
                     int32_t result,
                     const PP_NetAddress_Private& addr);
  void SendRecvFromResult(int32_t result,
                          const std::string& data,
                          const PP_NetAddress_Private& addr);
  void SendSendToReply(const ppapi::host::ReplyMessageContext& context,
                       int32_t result,
                       int32_t bytes_written);

  void SendBindError(const ppapi::host::ReplyMessageContext& context,
                     int32_t result);
  void SendRecvFromError(int32_t result);
  void SendSendToError(const ppapi::host::ReplyMessageContext& context,
                       int32_t result);

  // Bitwise-or of SocketOption flags. This stores the state about whether
  // each option is set before Bind() is called.
  int socket_options_;

  // Locally cached value of buffer size.
  int32_t rcvbuf_size_;
  int32_t sndbuf_size_;

  scoped_ptr<net::UDPSocket> socket_;
  bool closed_;

  scoped_refptr<net::IOBuffer> recvfrom_buffer_;
  scoped_refptr<net::IOBufferWithSize> sendto_buffer_;

  net::IPEndPoint recvfrom_address_;

  size_t remaining_recv_slots_;

  bool external_plugin_;
  bool private_api_;

  int render_process_id_;
  int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(PepperUDPSocketMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_MESSAGE_FILTER_H_
