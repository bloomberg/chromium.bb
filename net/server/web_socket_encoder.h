// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_WEB_SOCKET_ENCODER_H_
#define NET_SERVER_WEB_SOCKET_ENCODER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/server/web_socket.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_inflater.h"

namespace net {

class WebSocketEncoder {
 public:
  ~WebSocketEncoder();

  static WebSocketEncoder* CreateServer(const std::string& request_extensions,
                                        std::string* response_extensions);

  static const char kClientExtensions[];
  static WebSocketEncoder* CreateClient(const std::string& response_extensions);

  WebSocket::ParseResult DecodeFrame(const base::StringPiece& frame,
                                     int* bytes_consumed,
                                     std::string* output);

  void EncodeFrame(const std::string& frame,
                   int masking_key,
                   std::string* output);

 private:
  explicit WebSocketEncoder(bool is_server);
  WebSocketEncoder(bool is_server,
                   int deflate_bits,
                   int inflate_bits,
                   bool no_context_takeover);

  // Parses a value in the Sec-WebSocket-Extensions header. If it contains a
  // single element of the permessage-deflate extension, stores the result of
  // parsing the parameters of the extension into the given variables.
  // Otherwise, returns with *deflate set to false.
  //
  // - If the client_max_window_bits parameter is missing, *client_window_bits
  //   defaults to 15.
  // - If the client_max_window_bits parameter has an invalid value,
  //   *client_window_bits will be set to 0.
  // - If the server_max_window_bits parameter is missing, *server_window_bits
  //   defaults to 15.
  // - If the server_max_window_bits parameter has an invalid value,
  //   *client_window_bits will be set to 0.
  //
  // TODO(tyoshino): Consider using a struct than taking a lot of pointers for
  // output.
  static void ParseExtensions(const std::string& extensions,
                              bool* deflate,
                              bool* has_client_window_bits,
                              int* client_window_bits,
                              int* server_window_bits,
                              bool* client_no_context_takeover,
                              bool* server_no_context_takeover);

  bool Inflate(std::string* message);
  bool Deflate(const std::string& message, std::string* output);

  scoped_ptr<WebSocketDeflater> deflater_;
  scoped_ptr<WebSocketInflater> inflater_;
  bool is_server_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEncoder);
};

}  // namespace net

#endif  // NET_SERVER_WEB_SOCKET_ENCODER_H_
