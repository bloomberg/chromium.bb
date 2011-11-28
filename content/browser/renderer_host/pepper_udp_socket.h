// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_UDP_SOCKET_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_UDP_SOCKET_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "ppapi/c/pp_stdint.h"

class PepperMessageFilter;
struct PP_NetAddress_Private;

namespace net {
class IOBuffer;
class UDPServerSocket;
}

// PepperUDPSocket is used by PepperMessageFilter to handle requests from
// the Pepper UDP socket API (PPB_UDPSocket_Private).
class PepperUDPSocket {
 public:
  PepperUDPSocket(PepperMessageFilter* manager,
                  int32 routing_id,
                  uint32 plugin_dispatcher_id,
                  uint32 socket_id);
  ~PepperUDPSocket();

  void Bind(const PP_NetAddress_Private& addr);
  void RecvFrom(int32_t num_bytes);
  void SendTo(const std::string& data, const PP_NetAddress_Private& addr);

 private:
  void SendBindACK(bool result);
  void SendRecvFromACKError();
  void SendSendToACKError();

  void OnRecvFromCompleted(int result);
  void OnSendToCompleted(int result);

  PepperMessageFilter* manager_;
  int32 routing_id_;
  uint32 plugin_dispatcher_id_;
  uint32 socket_id_;

  net::OldCompletionCallbackImpl<PepperUDPSocket> recvfrom_callback_;
  net::OldCompletionCallbackImpl<PepperUDPSocket> sendto_callback_;

  scoped_ptr<net::UDPServerSocket> socket_;

  scoped_refptr<net::IOBuffer> recvfrom_buffer_;
  scoped_refptr<net::IOBuffer> sendto_buffer_;

  net::IPEndPoint recvfrom_address_;

  DISALLOW_COPY_AND_ASSIGN(PepperUDPSocket);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_UDP_SOCKET_H_
