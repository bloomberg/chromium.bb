// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKETS_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKETS_HOST_H_

#include "base/id_map.h"
#include "content/browser/browser_message_filter.h"
#include "content/common/p2p_sockets.h"
#include "net/base/ip_endpoint.h"

class P2PSocketHost;

class P2PSocketsHost : public BrowserMessageFilter {
 public:
  P2PSocketsHost();
  virtual ~P2PSocketsHost();

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

  IDMap<P2PSocketHost, IDMapOwnPointer> sockets_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketsHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKETS_HOST_H_
