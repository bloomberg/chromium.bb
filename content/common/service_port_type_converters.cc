// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_port_type_converters.h"

namespace mojo {

// static
content::TransferredMessagePort
TypeConverter<content::TransferredMessagePort,
              content::mojom::MojoTransferredMessagePortPtr>::
    Convert(const content::mojom::MojoTransferredMessagePortPtr& input) {
  content::TransferredMessagePort output;
  output.id = input->id;
  output.send_messages_as_values = input->send_messages_as_values;
  return output;
}

// static
content::mojom::MojoTransferredMessagePortPtr
TypeConverter<content::mojom::MojoTransferredMessagePortPtr,
              content::TransferredMessagePort>::
    Convert(const content::TransferredMessagePort& input) {
  content::mojom::MojoTransferredMessagePortPtr output(
      content::mojom::MojoTransferredMessagePort::New());
  output->id = input.id;
  output->send_messages_as_values = input.send_messages_as_values;
  return output;
}

}  // namespace mojo
