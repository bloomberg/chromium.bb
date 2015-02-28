// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MESSAGE_PORT_TYPES_H_
#define CONTENT_PUBLIC_COMMON_MESSAGE_PORT_TYPES_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"

namespace content {

// Struct representing a message sent across a message port. This struct hides
// the fact that messages can be serialized/encoded in multiple ways from code
// that doesn't care about the message contents.
struct CONTENT_EXPORT MessagePortMessage {
  MessagePortMessage();
  explicit MessagePortMessage(const base::string16& message);
  explicit MessagePortMessage(scoped_ptr<base::Value> message);
  MessagePortMessage(const MessagePortMessage& other);
  MessagePortMessage& operator=(const MessagePortMessage& other);
  ~MessagePortMessage();

  bool is_string() const { return !message_as_string.empty(); }
  bool is_value() const { return !message_as_value.empty(); }
  const base::Value* as_value() const;

  // Message serialized using blink::WebSerializedScriptValue. Only one of
  // |message_as_string| and |message_as_value| should be non-empty.
  base::string16 message_as_string;
  // Message as a base::Value. This is either an empty list or a list
  // containing exactly one item.
  base::ListValue message_as_value;
};

struct TransferredMessagePort {
  int id = MSG_ROUTING_NONE;
  bool send_messages_as_values = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MESSAGE_PORT_TYPES_H_
