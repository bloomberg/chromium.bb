// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/spy/websocket_server.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/tcp_server_socket.h"
#include "url/gurl.h"

namespace mojo {

const int kNotConnected = -1;

#define MOJO_DEBUGGER_MESSAGE_FORMAT "\"url: %s\n," \
                                     "time: %02d:%02d:%02d\n,"\
                                     "bytes: %u\n,"\
                                     "fields: %u\n,"\
                                     "name: %u\n,"\
                                     "requestId: %llu\n,"\
                                     "type: %s\n,"\
                                     "end:\n\""

WebSocketServer::WebSocketServer(int port,
                                 mojo::ScopedMessagePipeHandle server_pipe)
    : port_(port),
      connection_id_(kNotConnected),
      spy_server_(MakeProxy<spy_api::SpyServer>(server_pipe.Pass())) {
  spy_server_.set_client(this);
}

WebSocketServer::~WebSocketServer() {
}

bool WebSocketServer::Start() {
  scoped_ptr<net::ServerSocket> server_socket(
      new net::TCPServerSocket(NULL, net::NetLog::Source()));
  server_socket->ListenWithAddressAndPort("0.0.0.0", port_, 1);
  web_server_.reset(new net::HttpServer(server_socket.Pass(), this));
  net::IPEndPoint address;
  int error = web_server_->GetLocalAddress(&address);
  port_ = address.port();
  return (error == net::OK);
}

void WebSocketServer::LogMessageInfo(
    const mojo::MojoRequestHeader& message_header,
    const GURL& url,
    const base::Time& message_time) {
  base::Time::Exploded exploded;
  message_time.LocalExplode(&exploded);

  std::string output_message = base::StringPrintf(
      MOJO_DEBUGGER_MESSAGE_FORMAT,
      url.spec().c_str(),
      exploded.hour,
      exploded.minute,
      exploded.second,
      static_cast<unsigned>(message_header.num_bytes),
      static_cast<unsigned>(message_header.num_fields),
      static_cast<unsigned>(message_header.name),
      static_cast<unsigned long long>(
          message_header.num_fields == 3 ? message_header.request_id
              : 0),
      message_header.flags != 0 ?
        (message_header.flags & mojo::kMessageExpectsResponse ?
            "Expects response" : "Response message")
        : "Not a request or response message");
  if (Connected()) {
    web_server_->SendOverWebSocket(connection_id_, output_message.c_str());
  } else {
    DVLOG(1) << output_message;
  }
}

void WebSocketServer::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  web_server_->Send500(connection_id, "websockets protocol only");
}

void WebSocketServer::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  if (connection_id_ != kNotConnected) {
    // Reject connection since we already have our client.
    web_server_->Close(connection_id);
    return;
  }
  // Accept the connection.
  web_server_->AcceptWebSocket(connection_id, info);
  connection_id_ = connection_id;
}

void WebSocketServer::OnWebSocketMessage(
    int connection_id,
    const std::string& data) {

  if (data == "\"start\"") {
    spy_api::VersionPtr ver = spy_api::Version::New();
    ver->v_major = 0;
    ver->v_minor = 1;
    spy_server_->StartSession(
        ver.Pass(),
        base::Bind(&WebSocketServer::OnStartSession, base::Unretained(this)));
  } else if (data == "\"stop\"") {
    spy_server_->StopSession(
        base::Bind(&WebSocketServer::OnSessionEnd, base::Unretained(this)));
  }
}

void WebSocketServer::OnFatalError(spy_api::Result result) {
  web_server_->SendOverWebSocket(connection_id_, "\"fatal error\"");
}

void WebSocketServer::OnClose(
    int connection_id) {
  if (connection_id != connection_id_)
    return;
  connection_id_ = kNotConnected;

  spy_server_->StopSession(
      base::Bind(&WebSocketServer::OnSessionEnd, base::Unretained(this)));
}

void WebSocketServer::OnSessionEnd(spy_api::Result result) {
  // Called when the spy session (not the websocket) ends.
}

void WebSocketServer::OnClientConnection(
  const mojo::String& name,
  uint32_t id,
  spy_api::ConnectionOptions options) {
  std::string cc("\"");
  cc += name.To<std::string>() + "\"";
  web_server_->SendOverWebSocket(connection_id_, cc);
}

void WebSocketServer::OnMessage(spy_api::MessagePtr message) {
}

void WebSocketServer::OnStartSession(spy_api::Result, mojo::String) {
  web_server_->SendOverWebSocket(connection_id_, "\"ok start\"");
}

bool WebSocketServer::Connected() const {
  return connection_id_ !=  kNotConnected;
}

}  // namespace mojo
