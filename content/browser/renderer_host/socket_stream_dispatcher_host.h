// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SOCKET_STREAM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_SOCKET_STREAM_DISPATCHER_HOST_H_
#pragma once

#include <vector>

#include "base/id_map.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "net/socket_stream/socket_stream.h"

class GURL;
class SocketStreamHost;

namespace content {
class ResourceContext;
}

// Dispatches ViewHostMsg_SocketStream_* messages sent from renderer.
// It also acts as SocketStream::Delegate so that it sends
// ViewMsg_SocketStream_* messages back to renderer.
class SocketStreamDispatcherHost : public BrowserMessageFilter,
                                   public net::SocketStream::Delegate {
 public:
  SocketStreamDispatcherHost(
      ResourceMessageFilter::URLRequestContextSelector* selector,
      const content::ResourceContext* resource_context);
  virtual ~SocketStreamDispatcherHost();

  // BrowserMessageFilter methods.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // The object died, so cancel and detach all requests associated with it.
  void CancelRequestsForProcess(int host_id);

  // SocketStream::Delegate methods.
  virtual void OnConnected(net::SocketStream* socket,
                           int max_pending_send_allowed) OVERRIDE;
  virtual void OnSentData(net::SocketStream* socket, int amount_sent) OVERRIDE;
  virtual void OnReceivedData(net::SocketStream* socket,
                              const char* data, int len) OVERRIDE;
  virtual void OnClose(net::SocketStream* socket) OVERRIDE;
  virtual bool CanGetCookies(net::SocketStream* socket,
                             const GURL& url) OVERRIDE;
  virtual bool CanSetCookie(net::SocketStream* request,
                            const GURL& url,
                            const std::string& cookie_line,
                            net::CookieOptions* options) OVERRIDE;

 private:
  // Message handlers called by OnMessageReceived.
  void OnConnect(const GURL& url, int socket_id);
  void OnSendData(int socket_id, const std::vector<char>& data);
  void OnCloseReq(int socket_id);

  void DeleteSocketStreamHost(int socket_id);

  net::URLRequestContext* GetURLRequestContext();

  IDMap<SocketStreamHost> hosts_;
  const scoped_ptr<ResourceMessageFilter::URLRequestContextSelector>
      url_request_context_selector_;
  const content::ResourceContext* resource_context_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamDispatcherHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_SOCKET_STREAM_DISPATCHER_HOST_H_
