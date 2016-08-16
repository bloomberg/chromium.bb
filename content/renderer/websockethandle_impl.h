// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBSOCKETHANDLE_IMPL_H_
#define CONTENT_RENDERER_WEBSOCKETHANDLE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "content/common/websocket.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandle.h"

namespace blink {
class InterfaceProvider;
class WebSecurityOrigin;
class WebString;
class WebURL;
}  // namespace blink

namespace content {

class WebSocketHandleImpl : public blink::WebSocketHandle,
                            public mojom::WebSocketClient {
 public:
  explicit WebSocketHandleImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // This method may optionally be called before connect() to specify a
  // specific InterfaceProvider to get a WebSocket instance. By default,
  // connect() will use blink::Platform::interfaceProvider().
  void Initialize(blink::InterfaceProvider* interface_provider);

  // blink::WebSocketHandle methods:
  void connect(const blink::WebURL& url,
               const blink::WebVector<blink::WebString>& protocols,
               const blink::WebSecurityOrigin& origin,
               const blink::WebURL& first_party_for_cookies,
               const blink::WebString& user_agent_override,
               blink::WebSocketHandleClient* client) override;
  void send(bool fin,
            WebSocketHandle::MessageType type,
            const char* data,
            size_t size) override;
  void flowControl(int64_t quota) override;
  void close(unsigned short code, const blink::WebString& reason) override;

 private:
  ~WebSocketHandleImpl() override;
  void Disconnect();
  void OnConnectionError();

  // mojom::WebSocketClient methods:
  void OnFailChannel(const std::string& reason) override;
  void OnStartOpeningHandshake(
      mojom::WebSocketHandshakeRequestPtr request) override;
  void OnFinishOpeningHandshake(
      mojom::WebSocketHandshakeResponsePtr response) override;
  void OnAddChannelResponse(const std::string& selected_protocol,
                            const std::string& extensions) override;
  void OnDataFrame(bool fin,
                   mojom::WebSocketMessageType type,
                   const std::vector<uint8_t>& data) override;
  void OnFlowControl(int64_t quota) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override;
  void OnClosingHandshake() override;

  blink::WebSocketHandleClient* client_;

  mojom::WebSocketPtr websocket_;
  mojo::Binding<mojom::WebSocketClient> client_binding_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool did_initialize_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandleImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBSOCKETHANDLE_IMPL_H_
