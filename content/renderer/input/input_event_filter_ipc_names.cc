// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input_messages.h"

#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(name, ...) \
  case name::ID:                    \
    return #name;

const char* GetInputMessageTypeName(const IPC::Message& message) {
  switch (message.type()) {
#undef CONTENT_COMMON_INPUT_MESSAGES_H_
#include "content/common/input_messages.h"
#ifndef CONTENT_COMMON_INPUT_MESSAGES_H_
#error "Failed to include content/common/input_messages.h"
#endif
    default:
      NOTREACHED() << "Invalid message type: " << message.type();
      break;
  };
  return "NonInputMsgType";
}
