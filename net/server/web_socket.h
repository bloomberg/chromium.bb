// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_WEB_SOCKET_H_
#define NET_SERVER_WEB_SOCKET_H_

#include <string>

#include "base/basictypes.h"
#include "base/strings/string_piece.h"

namespace net {

class HttpConnection;
class HttpServer;
class HttpServerRequestInfo;

class WebSocket {
 public:
  enum ParseResult {
    FRAME_OK,
    FRAME_INCOMPLETE,
    FRAME_CLOSE,
    FRAME_ERROR
  };

  static WebSocket* CreateWebSocket(HttpServer* server,
                                    HttpConnection* connection,
                                    const HttpServerRequestInfo& request,
                                    size_t* pos);

  static ParseResult DecodeFrameHybi17(const base::StringPiece& frame,
                                       bool client_frame,
                                       int* bytes_consumed,
                                       std::string* output);

  static std::string EncodeFrameHybi17(const std::string& data,
                                       int masking_key);

  virtual void Accept(const HttpServerRequestInfo& request) = 0;
  virtual ParseResult Read(std::string* message) = 0;
  virtual void Send(const std::string& message) = 0;
  virtual ~WebSocket() {}

 protected:
  WebSocket(HttpServer* server, HttpConnection* connection);

  HttpServer* const server_;
  HttpConnection* const connection_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace net

#endif  // NET_SERVER_WEB_SOCKET_H_
