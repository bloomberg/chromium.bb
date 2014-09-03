// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBSOCKETHANDLE_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBSOCKETHANDLE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/common/handle_watcher.h"
#include "mojo/services/public/interfaces/network/web_socket.mojom.h"
#include "third_party/WebKit/public/platform/WebSocketHandle.h"

namespace mojo {
class NetworkService;
class WebSocketClientImpl;

// Implements WebSocketHandle by talking to the mojo WebSocket interface.
class WebSocketHandleImpl : public blink::WebSocketHandle {
 public:
  explicit WebSocketHandleImpl(NetworkService* network_service);

 private:
  virtual ~WebSocketHandleImpl();

  // blink::WebSocketHandle methods:
  virtual void connect(const blink::WebURL& url,
                       const blink::WebVector<blink::WebString>& protocols,
                       const blink::WebSerializedOrigin& origin,
                       blink::WebSocketHandleClient*) OVERRIDE;
  virtual void send(bool fin,
                    MessageType,
                    const char* data,
                    size_t size) OVERRIDE;
  virtual void flowControl(int64_t quota) OVERRIDE;
  virtual void close(unsigned short code,
                     const blink::WebString& reason) OVERRIDE;

  WebSocketPtr web_socket_;
  scoped_ptr<WebSocketClientImpl> client_;
  // True if close() was called.
  bool did_close_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandleImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBSOCKETHANDLE_IMPL_H_
