// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEBSOCKET_H_
#define CONTENT_COMMON_WEBSOCKET_H_

namespace content {

// WebSocket data message types sent between the browser and renderer processes.
enum WebSocketMessageType {
  WEB_SOCKET_MESSAGE_TYPE_CONTINUATION = 0x0,
  WEB_SOCKET_MESSAGE_TYPE_TEXT = 0x1,
  WEB_SOCKET_MESSAGE_TYPE_BINARY = 0x2
};

}  // namespace content

#endif  // CONTENT_COMMON_WEBSOCKET_H_
