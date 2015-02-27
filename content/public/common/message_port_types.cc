// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/message_port_types.h"

#include "base/logging.h"

namespace content {

MessagePortMessage::MessagePortMessage() {
}

MessagePortMessage::MessagePortMessage(const base::string16& message)
    : message_as_string(message) {
}

MessagePortMessage::MessagePortMessage(scoped_ptr<base::Value> message) {
  message_as_value.Append(message.release());
}

MessagePortMessage::MessagePortMessage(const MessagePortMessage& other) {
  *this = other;
}

MessagePortMessage& MessagePortMessage::operator=(
    const MessagePortMessage& other) {
  message_as_string = other.message_as_string;
  message_as_value.Clear();
  const base::Value* value;
  if (other.message_as_value.Get(0, &value))
    message_as_value.Append(value->DeepCopy());
  return *this;
}

MessagePortMessage::~MessagePortMessage() {
}

const base::Value* MessagePortMessage::as_value() const {
  const base::Value* value = nullptr;
  bool result = message_as_value.Get(0, &value);
  DCHECK(result);
  return value;
}

}  // namespace content
