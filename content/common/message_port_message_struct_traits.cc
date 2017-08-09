// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/message_port_message_struct_traits.h"

namespace mojo {

bool StructTraits<content::mojom::MessagePortMessage::DataView,
                  content::MessagePortMessage>::
    Read(content::mojom::MessagePortMessage::DataView data,
         content::MessagePortMessage* out) {
  if (!data.ReadEncodedMessage(&out->owned_encoded_message) ||
      !data.ReadPorts(&out->ports))
    return false;

  out->encoded_message = mojo::ConstCArray<uint8_t>(
      out->owned_encoded_message.data(), out->owned_encoded_message.size());
  return true;
}

}  // namespace mojo
