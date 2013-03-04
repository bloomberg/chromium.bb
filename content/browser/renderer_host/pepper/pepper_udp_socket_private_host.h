// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_PRIVATE_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_PRIVATE_HOST_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/host/resource_host.h"

struct PP_NetAddress_Private;

namespace net {
class IOBuffer;
class IOBufferWithSize;
class UDPServerSocket;
}

namespace ppapi {
namespace host {
struct ReplyMessageContext;
}
}

namespace content {

class BrowserPpapiHostImpl;
struct SocketPermissionRequest;

class CONTENT_EXPORT PepperUDPSocketPrivateHost
    : public ppapi::host::ResourceHost {
 public:
  PepperUDPSocketPrivateHost(BrowserPpapiHostImpl* host,
                             PP_Instance instance,
                             PP_Resource resource);
  virtual ~PepperUDPSocketPrivateHost();

  // ppapi::host::ResourceHost implementation.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  typedef base::Callback<void(bool allowed)> RequestCallback;

  int32_t OnMsgSetBoolSocketFeature(
      const ppapi::host::HostMessageContext* context,
      int32_t name,
      bool value);
  int32_t OnMsgBind(const ppapi::host::HostMessageContext* context,
                    const PP_NetAddress_Private& addr);
  int32_t OnMsgRecvFrom(const ppapi::host::HostMessageContext* context,
                        int32_t num_bytes);
  int32_t OnMsgSendTo(const ppapi::host::HostMessageContext* context,
                      const std::string& data,
                      const PP_NetAddress_Private& addr);
  int32_t OnMsgClose(const ppapi::host::HostMessageContext* context);

  void DoBind(const PP_NetAddress_Private& addr, bool allowed);
  void DoSendTo(const PP_NetAddress_Private& addr, bool allowed);
  void Close();

  void OnBindCompleted(int result);
  void OnRecvFromCompleted(int result);
  void OnSendToCompleted(int result);

  void SendBindReply(bool succeeded,
                     const PP_NetAddress_Private& addr);
  void SendRecvFromReply(bool succeeded,
                         const std::string& data,
                         const PP_NetAddress_Private& addr);
  void SendSendToReply(bool succeeded, int32_t bytes_written);

  void SendBindError();
  void SendRecvFromError();
  void SendSendToError();

  void CheckSocketPermissionsAndReply(const SocketPermissionRequest& params,
                                      const RequestCallback& callback);

  bool closed() { return closed_; }

  bool allow_address_reuse_;
  bool allow_broadcast_;

  scoped_ptr<net::UDPServerSocket> socket_;
  bool closed_;

  scoped_refptr<net::IOBuffer> recvfrom_buffer_;
  scoped_refptr<net::IOBufferWithSize> sendto_buffer_;

  net::IPEndPoint recvfrom_address_;
  net::IPEndPoint bound_address_;

  scoped_ptr<ppapi::host::ReplyMessageContext> bind_context_;
  scoped_ptr<ppapi::host::ReplyMessageContext> recv_from_context_;
  scoped_ptr<ppapi::host::ReplyMessageContext> send_to_context_;

  BrowserPpapiHostImpl* host_;

  base::WeakPtrFactory<PepperUDPSocketPrivateHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperUDPSocketPrivateHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_PRIVATE_HOST_H_
