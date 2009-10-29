// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SOCKET_STREAM_DISPATCHER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_SOCKET_STREAM_DISPATCHER_HOST_H_

#include <vector>

#include "base/id_map.h"
#include "ipc/ipc_message.h"
#include "net/socket_stream/socket_stream.h"

class GURL;
class SocketStreamHost;

// Dispatches ViewHostMsg_SocketStream_* messages sent from renderer.
// It also acts as SocketStream::Delegate so that it sends
// ViewMsg_SocketStream_* messages back to renderer.
class SocketStreamDispatcherHost : public net::SocketStream::Delegate {
 public:
  SocketStreamDispatcherHost();
  virtual ~SocketStreamDispatcherHost();

  void Initialize(IPC::Message::Sender* sender, int process_id);
  bool OnMessageReceived(const IPC::Message& msg, bool* msg_ok);

  // SocketStream::Delegate methods.
  virtual void OnConnected(net::SocketStream* socket,
                           int max_pending_send_allowed);
  virtual void OnSentData(net::SocketStream* socket, int amount_sent);
  virtual void OnReceivedData(net::SocketStream* socket,
                              const char* data, int len);
  virtual void OnClose(net::SocketStream* socket);

  // For sync message.
  bool Send(IPC::Message* message) {
    return sender_->Send(message);
  }

 private:
  // Message handlers called by OnMessageReceived.
  void OnConnect(const GURL& url, int socket_id);
  void OnSendData(int socket_id, const std::vector<char>& data);
  void OnCloseReq(int socket_id);

  void DeleteSocketStreamHost(int socket_id);

  IPC::Message::Sender* sender_;
  int process_id_;
  IDMap<SocketStreamHost> hosts_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamDispatcherHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_SOCKET_STREAM_DISPATCHER_HOST_H_
