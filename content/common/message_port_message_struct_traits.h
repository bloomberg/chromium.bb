// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MESSAGE_PORT_MESSAGE_STRUCT_TRAITS_H_
#define CONTENT_COMMON_MESSAGE_PORT_MESSAGE_STRUCT_TRAITS_H_

#include "base/containers/span.h"
#include "content/common/message_port.mojom.h"
#include "content/common/message_port_message.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::MessagePortMessage::DataView,
                    content::MessagePortMessage> {
  static base::span<const uint8_t> encoded_message(
      content::MessagePortMessage& input) {
    return input.encoded_message;
  }

  static std::vector<mojo::ScopedMessagePipeHandle>& ports(
      content::MessagePortMessage& input) {
    return input.ports;
  }

  static bool Read(content::mojom::MessagePortMessage::DataView data,
                   content::MessagePortMessage* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_MESSAGE_PORT_MESSAGE_STRUCT_TRAITS_H_
