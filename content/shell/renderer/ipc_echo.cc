// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/ipc_echo.h"

#include "base/logging.h"
#include "content/shell/common/shell_messages.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/web/WebDOMCustomEvent.h"
#include "third_party/WebKit/public/web/WebDOMEvent.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"

namespace content {

IPCEcho::IPCEcho(blink::WebDocument document,
                                     IPC::Sender* sender,
                                     int routing_id)
    : document_(document), sender_(sender), routing_id_(routing_id),
      last_echo_id_(0) {
}

void IPCEcho::RequestEcho(int id, int size) {
  sender_->Send(new ShellViewHostMsg_EchoPing(
      routing_id_, id, std::string(size, '*')));
}

void IPCEcho::DidRespondEcho(int id, int size) {
  last_echo_id_ = id;
  last_echo_size_ = size;
  blink::WebString eventName = blink::WebString::fromUTF8("CustomEvent");
  blink::WebString eventType = blink::WebString::fromUTF8("pong");
  blink::WebDOMEvent event = document_.createEvent(eventName);
  event.to<blink::WebDOMCustomEvent>().initCustomEvent(
      eventType, false, false, blink::WebSerializedScriptValue());
  document_.dispatchEvent(event);
}

}  // namespace content
