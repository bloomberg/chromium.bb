// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_LISTEN_SOCKET_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_LISTEN_SOCKET_H_
#pragma once

#include <string>

#include "content/browser/debugger/devtools_remote_message.h"
#include "net/base/listen_socket.h"

class DevToolsRemoteListener;

// Listens to remote debugger incoming connections, handles the V8ARDP protocol
// socket input and invokes the message handler when appropriate.
class DevToolsRemoteListenSocket
    : public net::ListenSocket,
      public net::ListenSocket::ListenSocketDelegate {
 public:
  // Listen on port for the specified IP address.  Use 127.0.0.1 to only
  // accept local connections.
  static DevToolsRemoteListenSocket* Listen(
      const std::string& ip,
      int port,
      DevToolsRemoteListener* message_listener);

 protected:
  virtual void Listen();
  virtual void Accept();
  virtual void Close();
  virtual void SendInternal(const char* bytes, int len);

 private:
  virtual ~DevToolsRemoteListenSocket();

  // net::ListenSocket::ListenSocketDelegate interface
  virtual void DidAccept(net::ListenSocket *server,
                         net::ListenSocket *connection);
  virtual void DidRead(net::ListenSocket *connection,
                       const char* data, int len);
  virtual void DidClose(net::ListenSocket *connection);

  // The protocol states while reading socket input
  enum State {
    INVALID = 0,  // Bad handshake message received, retry
    HANDSHAKE = 1,  // Receiving handshake message
    HEADERS = 2,  // Receiving protocol headers
    PAYLOAD = 3  // Receiving payload
  };

  DevToolsRemoteListenSocket(SOCKET s,
                             DevToolsRemoteListener *listener);
  void StartNextField();
  void HandleMessage();
  void DispatchField();
  const std::string& GetHeader(const std::string& header_name,
                               const std::string& default_value) const;

  State state_;
  DevToolsRemoteMessage::HeaderMap header_map_;
  std::string protocol_field_;
  std::string payload_;
  int32 remaining_payload_length_;
  DevToolsRemoteListener* message_listener_;
  bool cr_received_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsRemoteListenSocket);
};

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_LISTEN_SOCKET_H_
