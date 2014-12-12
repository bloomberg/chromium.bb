// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/web_socket.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/md5.h"
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

namespace {

static uint32 WebSocketKeyFingerprint(const std::string& str) {
  std::string result;
  const char* p_char = str.c_str();
  int length = str.length();
  int spaces = 0;
  for (int i = 0; i < length; ++i) {
    if (p_char[i] >= '0' && p_char[i] <= '9')
      result.append(&p_char[i], 1);
    else if (p_char[i] == ' ')
      spaces++;
  }
  if (spaces == 0)
    return 0;
  int64 number = 0;
  if (!base::StringToInt64(result, &number))
    return 0;
  return base::HostToNet32(static_cast<uint32>(number / spaces));
}

class WebSocketHixie76 : public net::WebSocket {
 public:
  static net::WebSocket* Create(HttpServer* server,
                                HttpConnection* connection,
                                const HttpServerRequestInfo& request,
                                size_t* pos) {
    if (connection->read_buf()->GetSize() <
        static_cast<int>(*pos + kWebSocketHandshakeBodyLen))
      return NULL;
    return new WebSocketHixie76(server, connection, request, pos);
  }

  void Accept(const HttpServerRequestInfo& request) override {
    std::string key1 = request.GetHeaderValue("sec-websocket-key1");
    std::string key2 = request.GetHeaderValue("sec-websocket-key2");

    uint32 fp1 = WebSocketKeyFingerprint(key1);
    uint32 fp2 = WebSocketKeyFingerprint(key2);

    char data[16];
    memcpy(data, &fp1, 4);
    memcpy(data + 4, &fp2, 4);
    memcpy(data + 8, &key3_[0], 8);

    base::MD5Digest digest;
    base::MD5Sum(data, 16, &digest);

    std::string origin = request.GetHeaderValue("origin");
    std::string host = request.GetHeaderValue("host");
    std::string location = "ws://" + host + request.path;
    server_->SendRaw(
        connection_->id(),
        base::StringPrintf("HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
                           "Upgrade: WebSocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Origin: %s\r\n"
                           "Sec-WebSocket-Location: %s\r\n"
                           "\r\n",
                           origin.c_str(),
                           location.c_str()));
    server_->SendRaw(connection_->id(),
                     std::string(reinterpret_cast<char*>(digest.a), 16));
  }

  ParseResult Read(std::string* message) override {
    DCHECK(message);
    HttpConnection::ReadIOBuffer* read_buf = connection_->read_buf();
    if (read_buf->StartOfBuffer()[0])
      return FRAME_ERROR;

    base::StringPiece data(read_buf->StartOfBuffer(), read_buf->GetSize());
    size_t pos = data.find('\377', 1);
    if (pos == base::StringPiece::npos)
      return FRAME_INCOMPLETE;

    message->assign(data.data() + 1, pos - 1);
    read_buf->DidConsume(pos + 1);

    return FRAME_OK;
  }

  void Send(const std::string& message) override {
    char message_start = 0;
    char message_end = -1;
    server_->SendRaw(connection_->id(), std::string(1, message_start));
    server_->SendRaw(connection_->id(), message);
    server_->SendRaw(connection_->id(), std::string(1, message_end));
  }

 private:
  static const int kWebSocketHandshakeBodyLen;

  WebSocketHixie76(HttpServer* server,
                   HttpConnection* connection,
                   const HttpServerRequestInfo& request,
                   size_t* pos)
      : WebSocket(server, connection) {
    std::string key1 = request.GetHeaderValue("sec-websocket-key1");
    std::string key2 = request.GetHeaderValue("sec-websocket-key2");

    if (key1.empty()) {
      server->SendResponse(
          connection->id(),
          HttpServerResponseInfo::CreateFor500(
              "Invalid request format. Sec-WebSocket-Key1 is empty or isn't "
              "specified."));
      return;
    }

    if (key2.empty()) {
      server->SendResponse(
          connection->id(),
          HttpServerResponseInfo::CreateFor500(
              "Invalid request format. Sec-WebSocket-Key2 is empty or isn't "
              "specified."));
      return;
    }

    key3_.assign(connection->read_buf()->StartOfBuffer() + *pos,
                 kWebSocketHandshakeBodyLen);
    *pos += kWebSocketHandshakeBodyLen;
  }

  std::string key3_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHixie76);
};

const int WebSocketHixie76::kWebSocketHandshakeBodyLen = 8;

class WebSocketHybi17 : public WebSocket {
 public:
  static WebSocket* Create(HttpServer* server,
                           HttpConnection* connection,
                           const HttpServerRequestInfo& request,
                           size_t* pos) {
    std::string version = request.GetHeaderValue("sec-websocket-version");
    if (version != "8" && version != "13")
      return NULL;

    std::string key = request.GetHeaderValue("sec-websocket-key");
    if (key.empty()) {
      server->SendResponse(
          connection->id(),
          HttpServerResponseInfo::CreateFor500(
              "Invalid request format. Sec-WebSocket-Key is empty or isn't "
              "specified."));
      return NULL;
    }
    return new WebSocketHybi17(server, connection, request, pos);
  }

  void Accept(const HttpServerRequestInfo& request) override {
    static const char* const kWebSocketGuid =
        "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string key = request.GetHeaderValue("sec-websocket-key");
    std::string data = base::StringPrintf("%s%s", key.c_str(), kWebSocketGuid);
    std::string encoded_hash;
    base::Base64Encode(base::SHA1HashString(data), &encoded_hash);

    server_->SendRaw(connection_->id(),
                     base::StringPrintf(
                         "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
                         "Upgrade: WebSocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Accept: %s\r\n"
                         "%s"
                         "\r\n",
                         encoded_hash.c_str(), response_extensions_.c_str()));
  }

  ParseResult Read(std::string* message) override {
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

  void Send(const std::string& message) override {
    if (closed_)
      return;
    std::string encoded;
    encoder_->EncodeFrame(message, 0, &encoded);
    server_->SendRaw(connection_->id(), encoded);
  }

 private:
  WebSocketHybi17(HttpServer* server,
                  HttpConnection* connection,
                  const HttpServerRequestInfo& request,
                  size_t* pos)
      : WebSocket(server, connection),
        closed_(false) {
    std::string request_extensions =
        request.GetHeaderValue("sec-websocket-extensions");
    encoder_.reset(WebSocketEncoder::CreateServer(request_extensions,
                                                  &response_extensions_));
    if (!response_extensions_.empty()) {
      response_extensions_ =
          "Sec-WebSocket-Extensions: " + response_extensions_ + "\r\n";
    }
  }

  scoped_ptr<WebSocketEncoder> encoder_;
  std::string response_extensions_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHybi17);
};

}  // anonymous namespace

WebSocket* WebSocket::CreateWebSocket(HttpServer* server,
                                      HttpConnection* connection,
                                      const HttpServerRequestInfo& request,
                                      size_t* pos) {
  WebSocket* socket = WebSocketHybi17::Create(server, connection, request, pos);
  if (socket)
    return socket;

  return WebSocketHixie76::Create(server, connection, request, pos);
}

WebSocket::WebSocket(HttpServer* server, HttpConnection* connection)
    : server_(server),
      connection_(connection) {
}

WebSocket::~WebSocket() {
}

}  // namespace net
