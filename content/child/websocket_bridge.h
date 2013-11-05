// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBSOCKET_BRIDGE_H_
#define CONTENT_CHILD_WEBSOCKET_BRIDGE_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/websocket.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebSocketHandle.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

class WebSocketBridge : public WebKit::WebSocketHandle {
 public:
  WebSocketBridge();

  // Handles an IPC message from the browser process.
  bool OnMessageReceived(const IPC::Message& message);

  // WebSocketHandle functions.
  virtual void connect(const WebKit::WebURL& url,
                       const WebKit::WebVector<WebKit::WebString>& protocols,
                       const WebKit::WebString& origin,
                       WebKit::WebSocketHandleClient* client) OVERRIDE;
  virtual void send(bool fin,
                    WebSocketHandle::MessageType type,
                    const char* data,
                    size_t size) OVERRIDE;
  virtual void flowControl(int64_t quota) OVERRIDE;
  virtual void close(unsigned short code,
                     const WebKit::WebString& reason) OVERRIDE;

  virtual void Disconnect();

 private:
  virtual ~WebSocketBridge();

  void DidConnect(bool fail,
                  const std::string& selected_protocol,
                  const std::string& extensions);
  void DidFail(const std::string& message);
  void DidReceiveData(bool fin,
                      WebSocketMessageType type,
                      const std::vector<char>& data);
  void DidReceiveFlowControl(int64_t quota);
  void DidClose(bool was_clean, unsigned short code, const std::string& reason);

  int channel_id_;
  WebKit::WebSocketHandleClient* client_;

  static const int kInvalidChannelId = -1;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBSOCKET_BRIDGE_H_
