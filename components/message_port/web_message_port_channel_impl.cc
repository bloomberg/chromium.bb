// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/message_port/web_message_port_channel_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannelClient.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebMessagePortChannel;
using blink::WebMessagePortChannelArray;
using blink::WebMessagePortChannelClient;
using blink::WebString;

namespace message_port {

void WebMessagePortChannelImpl::CreatePair(
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  mojo::ScopedMessagePipeHandle pipe1;
  mojo::ScopedMessagePipeHandle pipe2;
  MojoResult result = mojo::CreateMessagePipe(nullptr, &pipe1, &pipe2);
  if (result != MOJO_RESULT_OK) {
    NOTREACHED();
    return;
  }

  *channel1 = new WebMessagePortChannelImpl(std::move(pipe1));
  *channel2 = new WebMessagePortChannelImpl(std::move(pipe2));
}

WebMessagePortChannelImpl::WebMessagePortChannelImpl(
    mojo::ScopedMessagePipeHandle pipe)
    : client_(nullptr), pipe_(std::move(pipe)) {
  handle_watcher_.Start(
      pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE,
      base::Bind(&WebMessagePortChannelImpl::OnMessageAvailable,
                 base::Unretained(this)));
}

WebMessagePortChannelImpl::~WebMessagePortChannelImpl() {
}

void WebMessagePortChannelImpl::setClient(WebMessagePortChannelClient* client) {
  client_ = client;
}

void WebMessagePortChannelImpl::destroy() {
  setClient(nullptr);
  delete this;
}

void WebMessagePortChannelImpl::postMessage(
    const WebString& message_as_string,
    WebMessagePortChannelArray* channels) {
  base::string16 string = message_as_string;

  std::vector<MojoHandle> handles;
  if (channels) {
    for (size_t i = 0; i < channels->size(); ++i) {
      WebMessagePortChannelImpl* channel =
          static_cast<WebMessagePortChannelImpl*>((*channels)[i]);
      handles.push_back(channel->pipe_.release().value());
      channel->handle_watcher_.Cancel();
    }
    delete channels;
  }

  uint32_t num_handles = static_cast<uint32_t>(handles.size());
  MojoHandle* handles_ptr = handles.empty() ? nullptr : &handles[0];

  MojoResult result = MojoWriteMessage(
      pipe_.get().value(), string.c_str(),
      static_cast<uint32_t>(string.length() * sizeof(base::char16)),
      handles_ptr, num_handles, MOJO_WRITE_MESSAGE_FLAG_NONE);
  DCHECK_EQ(MOJO_RESULT_OK, result);
}

bool WebMessagePortChannelImpl::tryGetMessage(
    WebString* message,
    WebMessagePortChannelArray& channels) {
  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;
  MojoResult result = MojoReadMessage(
      pipe_.get().value(), nullptr, &num_bytes, nullptr, &num_handles,
      MOJO_READ_MESSAGE_FLAG_NONE);
  if (result != MOJO_RESULT_RESOURCE_EXHAUSTED)
    return false;

  base::string16 message16;
  CHECK(num_bytes % sizeof(base::char16) == 0);
  message16.resize(num_bytes / sizeof(base::char16));
  std::vector<MojoHandle> handles;
  handles.resize(num_handles);

  MojoHandle* handles_ptr = handles.empty() ? nullptr : &handles[0];
  result = MojoReadMessage(
      pipe_.get().value(), &message16[0], &num_bytes, handles_ptr, &num_handles,
      MOJO_READ_MESSAGE_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    NOTREACHED();
    return false;
  }

  *message = message16;
  WebMessagePortChannelArray ports(handles.size());
  for (size_t i = 0; i < handles.size(); ++i) {
    mojo::MessagePipeHandle mph(handles[i]);
    mojo::ScopedMessagePipeHandle handle(mph);
    ports[i] = new WebMessagePortChannelImpl(std::move(handle));
  }
  channels = ports;
  return true;
}

void WebMessagePortChannelImpl::OnMessageAvailable(MojoResult result) {
  // |result| can be MOJO_RESULT_ABORTED when the message loop shuts down, or
  // MOJO_RESULT_FAILED_PRECONDITION when the end-of-file is reached.
  if (result == MOJO_RESULT_ABORTED ||
      result == MOJO_RESULT_FAILED_PRECONDITION)
    return;

  DCHECK_EQ(MOJO_RESULT_OK, result);
  if (!client_)
    return;
  client_->messageAvailable();
}

}  // namespace message_port
