// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/web_socket.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "net/server/http_connection.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/server/web_socket_encoder.h"

namespace net {

WebSocket::WebSocket(HttpServer* server,
                     HttpConnection* connection,
                     const HttpServerRequestInfo& request)
    : server_(server), connection_(connection), closed_(false) {
  std::string request_extensions =
      request.GetHeaderValue("sec-websocket-extensions");
  encoder_.reset(WebSocketEncoder::CreateServer(request_extensions,
                                                &response_extensions_));
  if (!response_extensions_.empty()) {
    response_extensions_ =
        "Sec-WebSocket-Extensions: " + response_extensions_ + "\r\n";
  }
}

WebSocket::~WebSocket() {}

scoped_ptr<WebSocket> WebSocket::CreateWebSocket(
    HttpServer* server,
    HttpConnection* connection,
    const HttpServerRequestInfo& request) {
  std::string version = request.GetHeaderValue("sec-websocket-version");
  if (version != "8" && version != "13") {
    server->SendResponse(
        connection->id(),
        HttpServerResponseInfo::CreateFor500(
            "Invalid request format. The version is not valid."));
    return nullptr;
  }

  std::string key = request.GetHeaderValue("sec-websocket-key");
  if (key.empty()) {
    server->SendResponse(
        connection->id(),
        HttpServerResponseInfo::CreateFor500(
            "Invalid request format. Sec-WebSocket-Key is empty or isn't "
            "specified."));
    return nullptr;
  }
  return make_scoped_ptr(new WebSocket(server, connection, request));
}

void WebSocket::Accept(const HttpServerRequestInfo& request) {
  static const char* const kWebSocketGuid =
      "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  std::string key = request.GetHeaderValue("sec-websocket-key");
  std::string data = base::StringPrintf("%s%s", key.c_str(), kWebSocketGuid);
  std::string encoded_hash;
  base::Base64Encode(base::SHA1HashString(data), &encoded_hash);

  server_->SendRaw(
      connection_->id(),
      base::StringPrintf("HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
                         "Upgrade: WebSocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Accept: %s\r\n"
                         "%s"
                         "\r\n",
                         encoded_hash.c_str(), response_extensions_.c_str()));
}

WebSocket::ParseResult WebSocket::Read(std::string* message) {
  HttpConnection::ReadIOBuffer* read_buf = connection_->read_buf();
  base::StringPiece frame(read_buf->StartOfBuffer(), read_buf->GetSize());
  int bytes_consumed = 0;
  ParseResult result = encoder_->DecodeFrame(frame, &bytes_consumed, message);
  if (result == FRAME_OK)
    read_buf->DidConsume(bytes_consumed);
  if (result == FRAME_CLOSE)
    closed_ = true;
  return result;
}

void WebSocket::Send(const std::string& message) {
  if (closed_)
    return;
  std::string encoded;
  encoder_->EncodeFrame(message, 0, &encoded);
  server_->SendRaw(connection_->id(), encoded);
}

}  // namespace net
