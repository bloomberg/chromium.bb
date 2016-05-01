// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGE_PORT_WEB_MESSAGE_PORT_CHANNEL_IMPL_H_
#define COMPONENTS_MESSAGE_PORT_WEB_MESSAGE_PORT_CHANNEL_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/watcher.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"

namespace message_port {

class WebMessagePortChannelImpl : public blink::WebMessagePortChannel {
 public:
  static void CreatePair(
      blink::WebMessagePortChannel** channel1,
      blink::WebMessagePortChannel** channel2);

 private:
  explicit WebMessagePortChannelImpl(mojo::ScopedMessagePipeHandle pipe);
  virtual ~WebMessagePortChannelImpl();

  // blink::WebMessagePortChannel implementation.
  void setClient(blink::WebMessagePortChannelClient* client) override;
  void destroy() override;
  void postMessage(const blink::WebString& message,
                   blink::WebMessagePortChannelArray* channels) override;
  bool tryGetMessage(blink::WebString* message,
                     blink::WebMessagePortChannelArray& channels) override;

  void OnMessageAvailable(MojoResult result);

  blink::WebMessagePortChannelClient* client_;
  mojo::ScopedMessagePipeHandle pipe_;
  mojo::Watcher handle_watcher_;

  DISALLOW_COPY_AND_ASSIGN(WebMessagePortChannelImpl);
};

}  // namespace message_port

#endif  // COMPONENTS_MESSAGE_PORT_WEB_MESSAGE_PORT_CHANNEL_IMPL_H_
