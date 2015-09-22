// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_WEB_SOCKET_H_
#define NET_SERVER_WEB_SOCKET_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"

namespace net {

class HttpConnection;
class HttpServer;
class HttpServerRequestInfo;
class WebSocketEncoder;

class WebSocket final {
 public:
  enum ParseResult {
    FRAME_OK,
    FRAME_INCOMPLETE,
    FRAME_CLOSE,
    FRAME_ERROR
  };

  static scoped_ptr<WebSocket> CreateWebSocket(
      HttpServer* server,
      HttpConnection* connection,
      const HttpServerRequestInfo& request);

  void Accept(const HttpServerRequestInfo& request);
  ParseResult Read(std::string* message);
  void Send(const std::string& message);
  ~WebSocket();

 private:
  WebSocket(HttpServer* server,
            HttpConnection* connection,
            const HttpServerRequestInfo& request);

  HttpServer* const server_;
  HttpConnection* const connection_;
  scoped_ptr<WebSocketEncoder> encoder_;
  std::string response_extensions_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace net

#endif  // NET_SERVER_WEB_SOCKET_H_
