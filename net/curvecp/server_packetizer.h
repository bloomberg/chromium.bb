// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_SERVER_PACKETIZER_H_
#define NET_CURVECP_SERVER_PACKETIZER_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/curvecp/packetizer.h"
#include "net/curvecp/protocol.h"

namespace net {

class IOBuffer;
class IPEndPoint;
class UDPServerSocket;

class ServerPacketizer : public base::RefCounted<ServerPacketizer>,
                         public Packetizer {
 public:
  ServerPacketizer();
  virtual ~ServerPacketizer();

  // Listen for new connections from the Packetizer.
  int Listen(const IPEndPoint& endpoint, Packetizer::Listener* listener);

  // Register a listener for a connection.
  // To revoke the registration, call Close().
  bool Open(ConnectionKey key, Packetizer::Listener* listener);

  // Packetizer methods
  virtual int SendMessage(ConnectionKey key,
                          const char* data,
                          size_t length,
                          OldCompletionCallback* callback) OVERRIDE;
  virtual void Close(ConnectionKey key) OVERRIDE;
  virtual int GetPeerAddress(IPEndPoint* endpoint) const OVERRIDE;
  virtual int max_message_payload() const OVERRIDE;

 private:
  enum State {
    NONE,       // The initial state, before listen.
    LISTENING,  // Listening for packets.
  };

  typedef std::map<ConnectionKey, Packetizer::Listener*> ListenerMap;
  typedef std::map<ConnectionKey, IPEndPoint> ConnectionMap;

  // Callbacks when an internal IO is completed.
  void OnReadComplete(int result);
  void OnWriteComplete(int result);

  // Process the result of a Read operation.
  void ProcessRead(int bytes_read);

  // Read packets until an error occurs.
  int ReadPackets();

  // Handlers for recognized packets.
  void HandleHelloPacket(Packet* packet, int length);
  void HandleInitiatePacket(Packet* packet, int length);
  void HandleClientMessagePacket(Packet* packet, int length);
  void HandleMessage(ConnectionKey key, unsigned char* message, int length);

  // Manage the list of known "connections".
  void AddConnection(ConnectionKey key, const IPEndPoint& endpoint);
  // TODO(mbelshe): need to trim out aged/idle connections
  void RemoveConnection(ConnectionKey key);
  bool IsActiveConnection(ConnectionKey key);

  State state_;
  scoped_ptr<UDPServerSocket> socket_;
  Packetizer::Listener* listener_;  // Accept listener.

  scoped_refptr<IOBuffer> read_buffer_;
  IPEndPoint recv_address_;

  // The connection map tracks active client addresses which are known
  // to this server.
  ConnectionMap connection_map_;
  // The listener map tracks active message listeners known to the packetizer.
  ListenerMap listener_map_;

  OldCompletionCallbackImpl<ServerPacketizer> read_callback_;
  OldCompletionCallbackImpl<ServerPacketizer> write_callback_;

  DISALLOW_COPY_AND_ASSIGN(ServerPacketizer);
};

}  // namespace net

#endif  // NET_CURVECP_SERVER_PACKETIZER_H_
