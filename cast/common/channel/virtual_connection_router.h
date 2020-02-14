// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_ROUTER_H_
#define CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_ROUTER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/proto/cast_channel.pb.h"

namespace openscreen {
namespace cast {

class CastMessageHandler;
struct VirtualConnection;
class VirtualConnectionManager;

// Handles CastSockets by routing received messages to appropriate message
// handlers based on the VirtualConnection's local ID and sending messages over
// the appropriate CastSocket for a VirtualConnection.
//
// Basic model for using this would be:
//
// 1. Foo is a SenderSocketFactory::Client.
//
// 2. Foo calls SenderSocketFactory::Connect, optionally with VCRouter as the
//    CastSocket::Client.
//
// 3. Foo gets OnConnected callback and makes whatever local notes it needs
//    (e.g.  sink "resolved", init app probing state, etc.), then calls
//    VCRouter::TakeSocket.
//
// 4. Anything Foo wants to send (launch, app availability, etc.) goes through
//    VCRouter::SendMessage via an appropriate VC.  The virtual connection is
//    not created automatically, so Foo should either ensure its existence via
//    logic or check with the VirtualConnectionManager first.
class VirtualConnectionRouter final : public CastSocket::Client {
 public:
  class SocketErrorHandler {
   public:
    virtual void OnClose(CastSocket* socket) = 0;
    virtual void OnError(CastSocket* socket, Error error) = 0;
  };

  explicit VirtualConnectionRouter(VirtualConnectionManager* vc_manager);
  ~VirtualConnectionRouter() override;

  // These return whether the given |local_id| was successfully added/removed.
  bool AddHandlerForLocalId(std::string local_id, CastMessageHandler* endpoint);
  bool RemoveHandlerForLocalId(const std::string& local_id);

  // |error_handler| must live until either its OnError or OnClose is called.
  void TakeSocket(SocketErrorHandler* error_handler,
                  std::unique_ptr<CastSocket> socket);
  void CloseSocket(int id);

  Error SendMessage(VirtualConnection virtual_conn,
                    ::cast::channel::CastMessage message);

  // CastSocket::Client overrides.
  void OnError(CastSocket* socket, Error error) override;
  void OnMessage(CastSocket* socket,
                 ::cast::channel::CastMessage message) override;

 private:
  struct SocketWithHandler {
    std::unique_ptr<CastSocket> socket;
    SocketErrorHandler* error_handler;
  };

  VirtualConnectionManager* const vc_manager_;
  std::map<int, SocketWithHandler> sockets_;
  std::map<std::string /* local_id */, CastMessageHandler*> endpoints_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_ROUTER_H_
