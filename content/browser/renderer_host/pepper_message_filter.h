// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_MESSAGE_FILTER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/process.h"
#include "base/time.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/font_list_async.h"

struct PP_NetAddress_Private;

namespace content {
class ResourceContext;
}

namespace net {
class AddressList;
class HostResolver;
}

// This class is used in two contexts, both supporting PPAPI plugins. The first
// is on the renderer->browser channel, to handle requests from in-process
// PPAPI plugins and any requests that the PPAPI implementation code in the
// renderer needs to make. The second is on the plugin->browser channel to
// handle requests that out-of-process plugins send directly to the browser.
class PepperMessageFilter : public BrowserMessageFilter {
 public:
  explicit PepperMessageFilter(
     const content::ResourceContext* resource_context);
  explicit PepperMessageFilter(net::HostResolver* host_resolver);
  virtual ~PepperMessageFilter();

  // BrowserMessageFilter methods.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  template<class SocketType> class SocketManager;
  class TCPSocket;
  class TCPSocketManager;
  class UDPSocket;
  class UDPSocketManager;

#if defined(ENABLE_FLAPPER_HACKS)
  // Message handlers.
  void OnConnectTcp(int routing_id,
                    int request_id,
                    const std::string& host,
                    uint16 port);
  void OnConnectTcpAddress(int routing_id,
                           int request_id,
                           const PP_NetAddress_Private& address);

  // |Send()| a |PepperMsg_ConnectTcpACK|, which reports an error.
  bool SendConnectTcpACKError(int routing_id,
                              int request_id);

  // Used by |OnConnectTcp()| (below).
  class LookupRequest;
  friend class LookupRequest;

  // Continuation of |OnConnectTcp()|.
  void ConnectTcpLookupFinished(int routing_id,
                                int request_id,
                                const net::AddressList& addresses);
  void ConnectTcpOnWorkerThread(int routing_id,
                                int request_id,
                                net::AddressList addresses);

  // Continuation of |OnConnectTcpAddress()|.
  void ConnectTcpAddressOnWorkerThread(int routing_id,
                                       int request_id,
                                       PP_NetAddress_Private addr);
#endif  // ENABLE_FLAPPER_HACKS

  void OnGetLocalTimeZoneOffset(base::Time t, double* result);
  void OnGetFontFamilies(IPC::Message* reply);

  // Callback when the font list has been retrieved on a background thread.
  void GetFontFamiliesComplete(IPC::Message* reply_msg,
                               scoped_refptr<content::FontListResult> result);

  // Returns the host resolver (it may come from the resource context or the
  // host_resolver_ member).
  net::HostResolver* GetHostResolver();

  // When non-NULL, this should be used instead of the host_resolver_.
  const content::ResourceContext* const resource_context_;

  // When non-NULL, this should be used instead of the resource_context_. Use
  // GetHostResolver instead of accessing directly.
  net::HostResolver* host_resolver_;

  scoped_ptr<TCPSocketManager> socket_manager_tcp_;
  scoped_ptr<UDPSocketManager> socket_manager_udp_;

  DISALLOW_COPY_AND_ASSIGN(PepperMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_MESSAGE_FILTER_H_
