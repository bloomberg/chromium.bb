// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBMESSAGEPORTCHANNEL_IMPL_H_
#define CONTENT_CHILD_WEBMESSAGEPORTCHANNEL_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/common/message_port.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"

namespace content {

// This is thread safe.
class WebMessagePortChannelImpl : public blink::WebMessagePortChannel {
 public:
  ~WebMessagePortChannelImpl() override;
  explicit WebMessagePortChannelImpl(MessagePort message_port);

  static void CreatePair(blink::WebMessagePortChannel** channel1,
                         blink::WebMessagePortChannel** channel2);

  // Extracts MessagePorts for passing on to other processes.
  static std::vector<MessagePort> ExtractMessagePorts(
      blink::WebMessagePortChannelArray channels);

  // Creates WebMessagePortChannelImpl instances for MessagePorts passed in from
  // other processes.
  static blink::WebMessagePortChannelArray CreateFromMessagePorts(
      const std::vector<MessagePort>& message_ports);
  static blink::WebMessagePortChannelArray CreateFromMessagePipeHandles(
      std::vector<mojo::ScopedMessagePipeHandle> handles);

  MessagePort ReleaseMessagePort();

 private:
  explicit WebMessagePortChannelImpl(mojo::ScopedMessagePipeHandle handle);

  // WebMessagePortChannel implementation.
  void setClient(blink::WebMessagePortChannelClient* client) override;
  void postMessage(const blink::WebString& encoded_message,
                   blink::WebMessagePortChannelArray channels) override;
  bool tryGetMessage(blink::WebString* encoded_message,
                     blink::WebMessagePortChannelArray& channels) override;

  MessagePort port_;

  DISALLOW_COPY_AND_ASSIGN(WebMessagePortChannelImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBMESSAGEPORTCHANNEL_IMPL_H_
