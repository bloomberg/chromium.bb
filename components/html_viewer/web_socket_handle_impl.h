// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_SOCKET_HANDLE_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_SOCKET_HANDLE_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/message_pump/handle_watcher.h"
#include "mojo/services/network/public/interfaces/web_socket.mojom.h"
#include "third_party/WebKit/public/platform/WebSocketHandle.h"

namespace mojo {
class WebSocketFactory;
class WebSocketWriteQueue;
}

namespace html_viewer {

class WebSocketClientImpl;

// Implements WebSocketHandle by talking to the mojo WebSocket interface.
class WebSocketHandleImpl : public blink::WebSocketHandle {
 public:
  explicit WebSocketHandleImpl(mojo::WebSocketFactory* factory);

 private:
  friend class WebSocketClientImpl;

  virtual ~WebSocketHandleImpl();

  // blink::WebSocketHandle methods:
  virtual void connect(const blink::WebURL& url,
                       const blink::WebVector<blink::WebString>& protocols,
                       const blink::WebSecurityOrigin& origin,
                       blink::WebSocketHandleClient*);
  virtual void send(bool fin, MessageType, const char* data, size_t size);
  virtual void flowControl(int64_t quota);
  virtual void close(unsigned short code, const blink::WebString& reason);

  // Called when we finished writing to |send_stream_|.
  void DidWriteToSendStream(bool fin,
                            WebSocketHandle::MessageType type,
                            uint32_t num_bytes,
                            const char* data);

  // Called when the socket is closed.
  void Disconnect();

  mojo::WebSocketPtr web_socket_;
  scoped_ptr<WebSocketClientImpl> client_;
  mojo::ScopedDataPipeProducerHandle send_stream_;
  scoped_ptr<mojo::WebSocketWriteQueue> write_queue_;
  // True if close() was called.
  bool did_close_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandleImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_SOCKET_HANDLE_IMPL_H_
