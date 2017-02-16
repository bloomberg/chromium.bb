// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webmessageportchannel_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannelClient.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebMessagePortChannel;
using blink::WebMessagePortChannelArray;
using blink::WebMessagePortChannelClient;
using blink::WebString;

namespace content {

WebMessagePortChannelImpl::~WebMessagePortChannelImpl() {
  setClient(nullptr);
}

WebMessagePortChannelImpl::WebMessagePortChannelImpl(
    MessagePort message_port)
    : port_(message_port.ReleaseHandle()) {
}

// static
void WebMessagePortChannelImpl::CreatePair(
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  mojo::MessagePipe pipe;
  *channel1 = new WebMessagePortChannelImpl(std::move(pipe.handle0));
  *channel2 = new WebMessagePortChannelImpl(std::move(pipe.handle1));
}

// static
std::vector<MessagePort>
WebMessagePortChannelImpl::ExtractMessagePorts(
    WebMessagePortChannelArray channels) {
  std::vector<MessagePort> message_ports(channels.size());
  for (size_t i = 0; i < channels.size(); ++i) {
    WebMessagePortChannelImpl* channel_impl =
        static_cast<WebMessagePortChannelImpl*>(channels[i].get());
    message_ports[i] = channel_impl->ReleaseMessagePort();
    DCHECK(message_ports[i].GetHandle().is_valid());
  }
  return message_ports;
}

// static
WebMessagePortChannelArray
WebMessagePortChannelImpl::CreateFromMessagePorts(
    const std::vector<MessagePort>& message_ports) {
  WebMessagePortChannelArray channels(message_ports.size());
  for (size_t i = 0; i < message_ports.size(); ++i)
    channels[i] = base::MakeUnique<WebMessagePortChannelImpl>(message_ports[i]);
  return channels;
}

// static
WebMessagePortChannelArray
WebMessagePortChannelImpl::CreateFromMessagePipeHandles(
    std::vector<mojo::ScopedMessagePipeHandle> handles) {
  WebMessagePortChannelArray channels(handles.size());
  for (size_t i = 0; i < handles.size(); ++i) {
    channels[i] = base::MakeUnique<WebMessagePortChannelImpl>(
        MessagePort(std::move(handles[i])));
  }
  return channels;
}

MessagePort WebMessagePortChannelImpl::ReleaseMessagePort() {
  return MessagePort(port_.ReleaseHandle());
}

WebMessagePortChannelImpl::WebMessagePortChannelImpl(
    mojo::ScopedMessagePipeHandle handle)
    : port_(std::move(handle)) {
}

void WebMessagePortChannelImpl::setClient(WebMessagePortChannelClient* client) {
  if (client) {
    port_.SetCallback(
        base::Bind(&WebMessagePortChannelClient::messageAvailable,
                   base::Unretained(client)));
  } else {
    port_.ClearCallback();
  }
}

void WebMessagePortChannelImpl::postMessage(
    const WebString& encoded_message,
    WebMessagePortChannelArray channels) {
  std::vector<MessagePort> ports;
  if (!channels.isEmpty()) {
    ports.resize(channels.size());
    for (size_t i = 0; i < channels.size(); ++i) {
      ports[i] = static_cast<WebMessagePortChannelImpl*>(channels[i].get())->
          ReleaseMessagePort();
    }
  }
  port_.PostMessage(encoded_message.utf16(), std::move(ports));
}

bool WebMessagePortChannelImpl::tryGetMessage(
    WebString* encoded_message,
    WebMessagePortChannelArray& channels) {
  base::string16 buffer;
  std::vector<MessagePort> ports;
  if (!port_.GetMessage(&buffer, &ports))
    return false;

  *encoded_message = WebString::fromUTF16(buffer);

  if (!ports.empty()) {
    channels = WebMessagePortChannelArray(ports.size());
    for (size_t i = 0; i < ports.size(); ++i)
      channels[i] = base::MakeUnique<WebMessagePortChannelImpl>(ports[i]);
  }
  return true;
}

}  // namespace content
