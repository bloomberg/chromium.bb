// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/socket_stream_dispatcher_host.h"

#include "base/logging.h"
#include "content/browser/renderer_host/socket_stream_host.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/common/resource_messages.h"
#include "content/common/socket_stream.h"
#include "content/common/socket_stream_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/websockets/websocket_job.h"
#include "net/websockets/websocket_throttle.h"

SocketStreamDispatcherHost::SocketStreamDispatcherHost(
    int render_process_id,
    ResourceMessageFilter::URLRequestContextSelector* selector,
    content::ResourceContext* resource_context)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ssl_delegate_weak_factory_(this)),
      render_process_id_(render_process_id),
      url_request_context_selector_(selector),
      resource_context_(resource_context) {
  DCHECK(selector);
  net::WebSocketJob::EnsureInit();
}

bool SocketStreamDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                   bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SocketStreamDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(SocketStreamHostMsg_Connect, OnConnect)
    IPC_MESSAGE_HANDLER(SocketStreamHostMsg_SendData, OnSendData)
    IPC_MESSAGE_HANDLER(SocketStreamHostMsg_Close, OnCloseReq)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

// SocketStream::Delegate methods implementations.
void SocketStreamDispatcherHost::OnConnected(net::SocketStream* socket,
                                             int max_pending_send_allowed) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(1) << "SocketStreamDispatcherHost::OnConnected socket_id=" << socket_id
           << " max_pending_send_allowed=" << max_pending_send_allowed;
  if (socket_id == content::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnConnected";
    return;
  }
  if (!Send(new SocketStreamMsg_Connected(
          socket_id, max_pending_send_allowed))) {
    LOG(ERROR) << "SocketStreamMsg_Connected failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnSentData(net::SocketStream* socket,
                                            int amount_sent) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(1) << "SocketStreamDispatcherHost::OnSentData socket_id=" << socket_id
           << " amount_sent=" << amount_sent;
  if (socket_id == content::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnSentData";
    return;
  }
  if (!Send(new SocketStreamMsg_SentData(socket_id, amount_sent))) {
    LOG(ERROR) << "SocketStreamMsg_SentData failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnReceivedData(
    net::SocketStream* socket, const char* data, int len) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(1) << "SocketStreamDispatcherHost::OnReceiveData socket_id="
           << socket_id;
  if (socket_id == content::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnReceivedData";
    return;
  }
  if (!Send(new SocketStreamMsg_ReceivedData(
          socket_id, std::vector<char>(data, data + len)))) {
    LOG(ERROR) << "SocketStreamMsg_ReceivedData failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnClose(net::SocketStream* socket) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(1) << "SocketStreamDispatcherHost::OnClosed socket_id=" << socket_id;
  if (socket_id == content::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnClose";
    return;
  }
  DeleteSocketStreamHost(socket_id);
}

void SocketStreamDispatcherHost::OnSSLCertificateError(
    net::SocketStream* socket, const net::SSLInfo& ssl_info, bool fatal) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(1) << "SocketStreamDispatcherHost::OnSSLCertificateError socket_id="
           << socket_id;
  if (socket_id == content::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnSSLCertificateError";
    return;
  }
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  content::GlobalRequestID request_id(-1, socket_id);
  SSLManager::OnSSLCertificateError(ssl_delegate_weak_factory_.GetWeakPtr(),
      request_id, ResourceType::SUB_RESOURCE, socket->url(),
      render_process_id_, socket_stream_host->render_view_id(), ssl_info,
      fatal);
}

bool SocketStreamDispatcherHost::CanGetCookies(net::SocketStream* socket,
                                               const GURL& url) {
  return content::GetContentClient()->browser()->AllowGetCookie(
      url, url, net::CookieList(), resource_context_, 0, MSG_ROUTING_NONE);
}

bool SocketStreamDispatcherHost::CanSetCookie(net::SocketStream* request,
                                              const GURL& url,
                                              const std::string& cookie_line,
                                              net::CookieOptions* options) {
  return content::GetContentClient()->browser()->AllowSetCookie(
      url, url, cookie_line, resource_context_, 0, MSG_ROUTING_NONE, options);
}

void SocketStreamDispatcherHost::CancelSSLRequest(
    const content::GlobalRequestID& id,
    int error,
    const net::SSLInfo* ssl_info) {
  int socket_id = id.request_id;
  DVLOG(1) << "SocketStreamDispatcherHost::CancelSSLRequest socket_id="
           << socket_id;
  DCHECK_NE(content::kNoSocketId, socket_id);
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  if (ssl_info)
    socket_stream_host->CancelWithSSLError(*ssl_info);
  else
    socket_stream_host->CancelWithError(error);
}

void SocketStreamDispatcherHost::ContinueSSLRequest(
    const content::GlobalRequestID& id) {
  int socket_id = id.request_id;
  DVLOG(1) << "SocketStreamDispatcherHost::ContinueSSLRequest socket_id="
           << socket_id;
  DCHECK_NE(content::kNoSocketId, socket_id);
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  socket_stream_host->ContinueDespiteError();
}

SocketStreamDispatcherHost::~SocketStreamDispatcherHost() {
  // TODO(ukai): Implement IDMap::RemoveAll().
  for (IDMap<SocketStreamHost>::const_iterator iter(&hosts_);
       !iter.IsAtEnd();
       iter.Advance()) {
    int socket_id = iter.GetCurrentKey();
    const SocketStreamHost* socket_stream_host = iter.GetCurrentValue();
    delete socket_stream_host;
    hosts_.Remove(socket_id);
  }
}

// Message handlers called by OnMessageReceived.
void SocketStreamDispatcherHost::OnConnect(int render_view_id,
                                           const GURL& url,
                                           int socket_id) {
  DVLOG(1) << "SocketStreamDispatcherHost::OnConnect"
           << " render_view_id=" << render_view_id
           << " url=" << url
           << " socket_id=" << socket_id;
  DCHECK_NE(content::kNoSocketId, socket_id);
  if (hosts_.Lookup(socket_id)) {
    LOG(ERROR) << "socket_id=" << socket_id << " already registered.";
    return;
  }
  SocketStreamHost* socket_stream_host =
      new SocketStreamHost(this, render_view_id, socket_id);
  hosts_.AddWithID(socket_stream_host, socket_id);
  socket_stream_host->Connect(url, GetURLRequestContext());
  DVLOG(1) << "SocketStreamDispatcherHost::OnConnect -> " << socket_id;
}

void SocketStreamDispatcherHost::OnSendData(
    int socket_id, const std::vector<char>& data) {
  DVLOG(1) << "SocketStreamDispatcherHost::OnSendData socket_id=" << socket_id;
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  if (!socket_stream_host) {
    LOG(ERROR) << "socket_id=" << socket_id << " already closed.";
    return;
  }
  if (!socket_stream_host->SendData(data)) {
    // Cannot accept more data to send.
    socket_stream_host->Close();
  }
}

void SocketStreamDispatcherHost::OnCloseReq(int socket_id) {
  DVLOG(1) << "SocketStreamDispatcherHost::OnCloseReq socket_id=" << socket_id;
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  if (!socket_stream_host)
    return;
  socket_stream_host->Close();
}

void SocketStreamDispatcherHost::DeleteSocketStreamHost(int socket_id) {
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  delete socket_stream_host;
  hosts_.Remove(socket_id);
  if (!Send(new SocketStreamMsg_Closed(socket_id))) {
    LOG(ERROR) << "SocketStreamMsg_Closed failed.";
  }
}

net::URLRequestContext* SocketStreamDispatcherHost::GetURLRequestContext() {
  return url_request_context_selector_->GetRequestContext(
      ResourceType::SUB_RESOURCE);
}
