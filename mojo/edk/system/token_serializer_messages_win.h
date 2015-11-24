// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_TOKEN_SERIALIZER_MESSAGES_WIN_H_
#define MOJO_EDK_SYSTEM_TOKEN_SERIALIZER_MESSAGES_WIN_H_

#include <stdint.h>

#include "base/compiler_specific.h"

namespace mojo {
namespace edk {

// This header defines the message format between ChildTokenSerializer and
// ParentTokenSerializer.

enum MessageId {
  // The reply is two HANDLEs.
  CREATE_PLATFORM_CHANNEL_PAIR = 0,
  // The reply is tokens of the same count of passed in handles.
  HANDLE_TO_TOKEN,
  // The reply is handles of the same count of passed in tokens.
  TOKEN_TO_HANDLE,
};

// Definitions of the raw bytes sent between ChildTokenSerializer and
// ParentTokenSerializer.

struct ALIGNAS(4) TokenSerializerMessage {
  uint32_t size;
  MessageId id;
  // Data, if any, follows.
  union {
    HANDLE handles[1];  // If HANDLE_TO_TOKEN.
    uint64_t tokens[1];  // If TOKEN_TO_HANDLE.
  };
};

const int kTokenSerializerMessageHeaderSize =
    sizeof(uint32_t) + sizeof(MessageId);

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_TOKEN_SERIALIZER_MESSAGES_WIN_H_
