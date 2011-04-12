// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_

#include "base/id_map.h"
#include "content/browser/browser_message_filter.h"
#include "content/common/p2p_sockets.h"
#include "net/base/ip_endpoint.h"

class P2PSocketHost;

class P2PSocketDispatcherHost : public BrowserMessageFilter {
 public:
  P2PSocketDispatcherHost();
  virtual ~P2PSocketDispatcherHost();

  // BrowserMessageFilter overrides.
  virtual void OnChannelClosing();
  virtual void OnDestruct() const;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

 private:
  void OnCreateSocket(const IPC::Message& msg, P2PSocketType type,
                      int socket_id, const net::IPEndPoint& remote_address);
  void OnSend(const IPC::Message& msg, int socket_id,
              const net::IPEndPoint& socket_address,
              const std::vector<char>& data);
  void OnDestroySocket(const IPC::Message& msg, int socket_id);

  // Helpers for OnCreateSocket().
  //
  // TODO(sergeyu): Remove these methods. |local_address| should be a
  // parameter in the CreateSocket() message.
  void GetLocalAddressAndCreateSocket(
      int routing_id, P2PSocketType type, int socket_id,
      const net::IPEndPoint& remote_address);
  void FinishCreateSocket(
      int routing_id, const net::IPEndPoint& local_address, P2PSocketType type,
      int socket_id, const net::IPEndPoint& remote_address);


  IDMap<P2PSocketHost, IDMapOwnPointer> sockets_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcherHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
